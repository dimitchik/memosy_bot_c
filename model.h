#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  int64_t id;
  const char *first_name;
  const char *last_name;
  const char *username;
  const char *language_code;
  bool is_bot;
} User;

typedef struct {
  int64_t id;
  const char *title;
  const char *username;
  const char *first_name;
  const char *last_name;
  const char *type;
} Chat;

typedef struct {
  const char *type;
  int32_t offset;
  int32_t length;
  const char *url;
} MessageEntity;

typedef struct {
  int64_t id;
  int64_t message_thread_id;
  User *from;
  Chat *chat;
  const char *text;
  MessageEntity *entities;
  int32_t date;
} Message;

#endif