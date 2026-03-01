FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    && rm -rf /var/lib/apt/lists/*

# Create challenge directory
WORKDIR /challenge

# Copy challenge files
COPY compiler_challenge.c .
COPY Makefile .
COPY README.md .

# Pre-compile the compiler for players
RUN make

# Create a non-root user for security
RUN useradd -m -s /bin/bash ctfuser && \
    chown -R ctfuser:ctfuser /challenge

# Switch to non-root user
USER ctfuser

# Set working directory
WORKDIR /challenge

# Default command shows README
CMD ["/bin/bash", "-c", "cat README.md && echo '' && echo 'Run ./compiler to start the challenge'"]
