
### variables ###

debug ?= 0
NAME := rooms
SRC_DIR := src
BUILD_DIR := build
TESTS_DIR := tests
LOGS_DIR := logs
BIN_DIR := bin
ZIP_DIR := zip

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))


### compiler ###

CC := gcc-14
CFLAGS := -Wall -Wextra -pedantic
LDFLAGS := -lm

ifeq ($(debug), 1)
	CFLAGS += -g -O0
else
	CFLAGS += -O3
endif


### targets ###

## src ##

all: dir $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(OBJS) -o $(BIN_DIR)/$@ $(LDFLAGS)  

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@


### directory opts ###

.PHONY: dir clean zip

zip:
	mkdir $(ZIP_DIR)
	zip -r $(ZIP_DIR)/$(NAME).zip $(SRC_DIR) makefile README.md

dir:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(LOGS_DIR)
	@rsync -a --include '*/' --exclude '*' $(SRC_DIR)/ $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(ZIP_DIR) $(LOGS_DIR)

