# Use Arch Linux as the base image
FROM archlinux/archlinux:latest

# Set the working directory
WORKDIR /app

# Update package lists and install necessary packages
RUN pacman -Sy --noconfirm --needed \
    base-devel \
    git \
    curl \
    pkg-config \
    json-c \
    ffmpeg \
    yt-dlp

# Copy the source code
COPY . .

# Build the application
RUN make all

# Make the executable runnable
RUN chmod +x build/main

# Create downloads directory with proper permissions
RUN mkdir -p /downloads && \
    chmod 777 /downloads

# Create and switch to non-root user
RUN useradd -m appuser && \
    chown -R appuser:appuser /app /downloads

USER appuser

# Set downloads as working directory
WORKDIR /downloads

# Set environment variable for yt-dlp output directory
ENV YT_DLP_OUTPUT="/downloads"

# Command to run when the container starts
CMD ["/app/build/main"]
