set(REDIS_VERSION "8.2.1")
set(REDIS_DIR "${CMAKE_SOURCE_DIR}/vendor/redis")
set(REDIS_DOWNLOAD "https://github.com/redis/redis/archive/refs/tags/${REDIS_VERSION}.tar.gz")

haio_fetch(redis "${REDIS_DOWNLOAD}" "${REDIS_DIR}" src/redis-cli.c)
