CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS ?= -Isrc -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=
LDLIBS ?=

ifeq ($(TEXT_ONLY),1)
CPPFLAGS += -DATM_NO_SQLITE
else
LDLIBS += -lsqlite3
endif

TARGET := atm
SRC := src/main.c src/auth.c src/system.c src/storage.c src/utils.c src/password.c src/account.c src/notify.c src/ui.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean fclean re test verify check sanitize reset-data demo

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LDLIBS) -o $@

src/%.o: src/%.c src/header.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

tests/bin/test_interest: tests/test_interest.c src/account.c src/header.h
	mkdir -p tests/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_interest.c src/account.c -o $@

tests/bin/test_password: tests/test_password.c src/password.c src/header.h
	mkdir -p tests/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -DATM_PASSWORD_TEST tests/test_password.c src/password.c -o $@

tests/bin/test_concurrency: tests/test_concurrency.c src/storage.c src/account.c src/header.h
	mkdir -p tests/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_concurrency.c src/storage.c src/account.c $(LDLIBS) -o $@

verify: $(TARGET) tests/bin/test_interest tests/bin/test_password tests/bin/test_concurrency
	bash tests/verify.sh

test: verify

check: clean all verify

sanitize: clean
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined $(SRC) $(LDLIBS) -o $(TARGET)
	ASAN_OPTIONS=detect_leaks=1 bash tests/core_flow.sh

reset-data:
	bash scripts/reset_data.sh

demo: $(TARGET)
	bash scripts/reset_data.sh
	./$(TARGET)

clean:
	rm -f $(OBJ)
	rm -rf tests/bin

fclean: clean
	rm -f $(TARGET) $(TARGET).exe

re: fclean all
