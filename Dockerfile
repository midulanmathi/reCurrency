# 1. Build Stage
FROM alpine:latest AS builder

# Install C++ tools AND Linux Headers (Crucial fix)
RUN apk add --no-cache g++ make linux-headers

# Copy source files
WORKDIR /src
COPY . .

# Compile
RUN make

# 2. Run Stage
FROM alpine:latest

# Install runtime libraries
RUN apk add --no-cache libstdc++

# Create data folder
RUN mkdir -p /data

# Copy executable
COPY --from=builder /src/recurrency /app/recurrency

WORKDIR /app

# Expose port
EXPOSE 18080

# Run
CMD ["./recurrency"]