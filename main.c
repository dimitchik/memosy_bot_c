#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <json-c/json.h>

#define ARENA_IMPLEMENTATION
#include "arena.h"

#include "model.h"

#define TOKEN_LENGTH 46
#define API_URL_TEMPLATE_LENGTH 29
#define API_URL_LENGTH (TOKEN_LENGTH + API_URL_TEMPLATE_LENGTH + 1)

static const char *API_URL_PREFIX = "https://api.telegram.org/bot";
static char token[TOKEN_LENGTH];
static char API_URL[API_URL_LENGTH];
static CURL *curl;

static Arena default_arena = {0};
static Arena *context_arena = &default_arena;

void *context_alloc(size_t size) {
  assert(context_arena);
  return arena_alloc(context_arena, size);
}

void context_reset() {
  assert(context_arena);
  arena_reset(context_arena);
}

void context_free() {
  assert(context_arena);
  arena_free(context_arena);
}

const char *method_url(const char *method, const char *params) {
  char *url =
      context_alloc(API_URL_LENGTH + strlen(method) + strlen(params) + 1);
  sprintf(url, "%s/%s%s", API_URL, method, params);
  return url;
}

static uint64_t offset = 0;

void log_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  fwrite(ptr, 1, size * nmemb, stdout);
  puts("");
}

void handle_url(const char *url, Message *message) {
  const char *last_slash = strrchr(url, '/');
  const char *filename = last_slash ? last_slash + 1 : url;
  size_t command_size =
      strlen("yt-dlp -o \"") + strlen(filename) +
      strlen(
          ".mp4\" --recode-video mp4 --max-filesize 50M --embed-thumbnail \"") +
      strlen(url) + strlen("\"") + 1;
  char *command = context_alloc(command_size);
  snprintf(command, command_size,
           "yt-dlp -o \"%s.mp4\" --recode-video mp4 --max-filesize 50M "
           "--embed-thumbnail \"%s\"",
           filename, url);
  int status = system(command);
  if (status != 0) {
    printf("Failed to download video: %s\n", url);
    return;
  } else {
    printf("Downloaded video: %s\n", url);
  }
  char *downloaded_file = context_alloc(strlen(filename) + strlen(".mp4") + 1);
  strcpy(downloaded_file, filename);
  strcat(downloaded_file, ".mp4");
  const char *const murl = method_url("sendVideo", "");
  curl_mime *mime = curl_mime_init(curl);
  curl_mimepart *part;
  CURL *temp_curl = curl_easy_init();
  // Add chat_id field
  char chat_id_str[32];
  snprintf(chat_id_str, sizeof(chat_id_str), "%lld", message->chat->id);
  part = curl_mime_addpart(mime);
  curl_mime_name(part, "chat_id");
  curl_mime_data(part, chat_id_str, CURL_ZERO_TERMINATED);

  // Reply to message
  // char reply_params[64];
  // snprintf(reply_params, sizeof(reply_params), "{\"message_id\":%d}",
  //          message->id);
  // part = curl_mime_addpart(mime);
  // curl_mime_name(part, "reply_parameters");
  // curl_mime_data(part, reply_params, CURL_ZERO_TERMINATED);

  ;
  const char *username = message->from->username;
  const char *first_name = message->from->first_name;
  const char *last_name = message->from->last_name;
  const char *message_text = message->text;
  const char *dangerous_chars = "_*[]()~`>#+-=|{}.!";
  char *escaped_message_text = NULL;
  if (message_text != NULL) {
    size_t escaped_text_len = 0;
    for (size_t i = 0; message_text[i] != '\0'; ++i) {
      if (strchr(dangerous_chars, message_text[i]) != NULL) {
        escaped_text_len += 2; // For backslash and the char itself
      } else {
        escaped_text_len += 1;
      }
    }
    escaped_message_text = context_alloc(escaped_text_len + 1);
    char *p = escaped_message_text;
    for (size_t i = 0; message_text[i] != '\0'; ++i) {
      if (strchr(dangerous_chars, message_text[i]) != NULL) {
        *p++ = '\\';
        *p++ = message_text[i];
      } else {
        *p++ = message_text[i];
      }
    }
    *p = '\0';
    message_text = escaped_message_text;
  }
  int64_t user_id = message->from->id;
  char user_link[256]; // Adjust size as needed
  const char *link_text;

  if (first_name != NULL && strlen(first_name) > 0) {
    link_text = context_alloc(strlen(first_name) + 1 +
                              (last_name != NULL ? strlen(last_name) + 1 : 1));
    sprintf((char *)link_text, "%s %s", first_name,
            last_name != NULL ? last_name : "");
  } else if (username != NULL && strlen(username) > 0) {
    link_text = username;
  } else {
    link_text = "Unknown user";
  }

  snprintf(user_link, sizeof(user_link), "tg://user?id=%lld", user_id);

  char *caption_str =
      context_alloc(strlen(link_text) + strlen(user_link) +
                    (message_text != NULL ? strlen(message_text) : 0) + 6);
  sprintf(caption_str, "[%s](%s)\n%s", link_text, user_link,
          message_text != NULL ? message_text : "");

  part = curl_mime_addpart(mime);
  curl_mime_name(part, "caption");
  curl_mime_data(part, caption_str, CURL_ZERO_TERMINATED);

  part = curl_mime_addpart(mime);
  curl_mime_name(part, "parse_mode");
  curl_mime_data(part, "MarkdownV2", CURL_ZERO_TERMINATED);

  // Add video file
  part = curl_mime_addpart(mime);
  curl_mime_name(part, "video");
  curl_mime_filedata(part, downloaded_file);

  curl_easy_setopt(temp_curl, CURLOPT_URL, murl);
  curl_easy_setopt(temp_curl, CURLOPT_TIMEOUT, 60);
  curl_easy_setopt(temp_curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(temp_curl, CURLOPT_WRITEFUNCTION, log_callback);
  CURLcode res = curl_easy_perform(temp_curl);

  curl_mime_free(mime);
  curl_easy_cleanup(temp_curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "failed to upload the file: %s\n", curl_easy_strerror(res));
    // return;
  }
  remove(strcat(filename, ".mp4"));
  const char *delete_message_method = "deleteMessage";
  char delete_message_params[256];
  snprintf(delete_message_params, sizeof(delete_message_params),
           "?chat_id=%lld&message_id=%lld", message->chat->id, message->id);
  const char *delete_message_url =
      method_url(delete_message_method, delete_message_params);
  CURL *delete_curl = curl_easy_init();
  curl_easy_setopt(delete_curl, CURLOPT_URL, delete_message_url);
  // curl_easy_setopt(delete_curl, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(delete_curl, CURLOPT_WRITEFUNCTION, log_callback);
  CURLcode delete_res = curl_easy_perform(delete_curl);
  printf("delete_res: %s\n", curl_easy_strerror(delete_res));
  if (delete_res != CURLE_OK) {
    fprintf(stderr, "Failed to delete message: %s\n",
            curl_easy_strerror(delete_res));
  }
  curl_easy_cleanup(delete_curl);
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  log_callback(ptr, size, nmemb, userdata);
  json_object *obj = json_tokener_parse(ptr);
  json_object *updates = json_object_object_get(obj, "result");
  if (!json_object_is_type(updates, json_type_array)) {
    return size * nmemb;
    // updates = json_object_new_array();
    // json_object_array_add(updates, json_object_object_get(obj, "result"));
  }
  size_t updates_length = json_object_array_length(updates);
  for (size_t i = 0; i < updates_length; ++i) {
    json_object *update = json_object_array_get_idx(updates, i);
    json_object *message_json = json_object_object_get(update, "message");
    if (!message_json) {
      json_object *update_id = json_object_object_get(update, "update_id");
      offset = json_object_get_uint64(update_id) + 1;
      continue;
    }
    Message *message = context_alloc(sizeof(Message));
    message->id = json_object_get_int64(
        json_object_object_get(message_json, "message_id"));
    message->message_thread_id = json_object_get_int64(
        json_object_object_get(message_json, "message_thread_id"));
    message->text =
        json_object_get_string(json_object_object_get(message_json, "text"));
    message->date =
        json_object_get_int(json_object_object_get(message_json, "date"));
    json_object *from_json = json_object_object_get(message_json, "from");
    message->from = context_alloc(sizeof(User));
    message->from->id =
        json_object_get_int64(json_object_object_get(from_json, "id"));
    message->from->is_bot =
        json_object_get_int(json_object_object_get(from_json, "is_bot"));
    message->from->first_name =
        json_object_get_string(json_object_object_get(from_json, "first_name"));
    message->from->last_name =
        json_object_get_string(json_object_object_get(from_json, "last_name"));
    message->from->username =
        json_object_get_string(json_object_object_get(from_json, "username"));
    message->from->language_code = json_object_get_string(
        json_object_object_get(from_json, "language_code"));
    message->chat = context_alloc(sizeof(Chat));
    json_object *chat_json = json_object_object_get(message_json, "chat");
    message->chat->id =
        json_object_get_int64(json_object_object_get(chat_json, "id"));
    message->chat->type =
        json_object_get_string(json_object_object_get(chat_json, "type"));
    message->chat->title =
        json_object_get_string(json_object_object_get(chat_json, "title"));
    message->chat->username =
        json_object_get_string(json_object_object_get(chat_json, "username"));
    message->chat->first_name =
        json_object_get_string(json_object_object_get(chat_json, "first_name"));
    message->chat->last_name =
        json_object_get_string(json_object_object_get(chat_json, "last_name"));
    json_object *entities_json =
        json_object_object_get(message_json, "entities");
    if (!entities_json) {
      json_object *update_id = json_object_object_get(update, "update_id");
      offset = json_object_get_uint64(update_id) + 1;
      continue;
    }
    if (message->text != NULL && strstr(message->text, "bot-ignore") != NULL) {
      json_object *update_id = json_object_object_get(update, "update_id");
      offset = json_object_get_uint64(update_id) + 1;
      continue;
    }
    message->entities = context_alloc(sizeof(MessageEntity) *
                                      json_object_array_length(entities_json));
    for (size_t i = 0; i < json_object_array_length(entities_json); i++) {
      json_object *entity = json_object_array_get_idx(entities_json, i);
      message->entities[i].type =
          json_object_get_string(json_object_object_get(entity, "type"));
      message->entities[i].offset =
          json_object_get_int(json_object_object_get(entity, "offset"));
      message->entities[i].length =
          json_object_get_int(json_object_object_get(entity, "length"));
      message->entities[i].url =
          json_object_get_string(json_object_object_get(entity, "url"));
      if (strcmp(message->entities[i].type, "text_link") == 0 ||
          strcmp(message->entities[i].type, "url") == 0) {
        char *download_url = context_alloc(message->entities[i].length + 1);
        strncpy(download_url, message->text + message->entities[i].offset,
                message->entities[i].length);
        download_url[message->entities[i].length] = '\0';

        handle_url(download_url, message);
      }
    }
    json_object *text = json_object_object_get(message_json, "text");
    json_object *update_id = json_object_object_get(update, "update_id");
    offset = json_object_get_uint64(update_id) + 1;
  }
  return size * nmemb;
};

static void atexit_handler() {
  context_free();
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

int main(void) {
  const char *env_token = getenv("GITHUB_TOKEN");
  if (env_token) {
    strcpy(token, env_token);
  } else {
    FILE *f = fopen("token", "r");
    if (!f) {
      fprintf(stderr, "Failed to read token\n");
      return EXIT_FAILURE;
    }
    fread(token, 1, 46, f);
    fclose(f);
  }
  sprintf(API_URL, "%s%s", API_URL_PREFIX, token);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize curl\n");
    return EXIT_FAILURE;
  }
  atexit(atexit_handler);
  while (1) {
    const char *const murl = method_url("getUpdates", "");
    curl_easy_setopt(curl, CURLOPT_URL, murl);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    char post_fields[100];
    sprintf(post_fields, "offset=%llu&limit=100&timeout=60", offset);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "Failed to perform curl: %s\n", curl_easy_strerror(res));
      return EXIT_FAILURE;
    }
    context_reset();
  }
  atexit_handler();
  return EXIT_SUCCESS;
}