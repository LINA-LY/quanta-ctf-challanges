FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc \
    libseccomp-dev \
    libseccomp2 \
    socat && \
    rm -rf /var/lib/apt/lists/*

# Create challenge user
RUN useradd -m -s /bin/bash ctf

# Set up challenge directory
WORKDIR /home/ctf

# Copy source and build files
COPY quantum_printer_v2.c .
COPY Makefile .

# Build the challenge
RUN make release && \
    chown root:root quantum_printer && \
    chmod 755 quantum_printer

# Copy flag
COPY flag.txt .
RUN chown root:root flag.txt && \
    chmod 444 flag.txt

# Clean up build artifacts
RUN rm quantum_printer_v2.c Makefile && \
    apt-get purge -y gcc libseccomp-dev && \
    apt-get autoremove -y && \
    apt-get clean

# Switch to challenge user
USER ctf

# Expose port
EXPOSE 1337

# Run with socat
CMD socat TCP-LISTEN:1337,reuseaddr,fork EXEC:./quantum_printer,stderr
