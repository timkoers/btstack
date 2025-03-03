CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
CFLAGS += -I.
		  
CFLAGS += -DHAVE_HAL_AUDIO

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/freertos

COMMON = \
    btstack_linked_list.c  \
	btstack_memory.c 	\
	btstack_run_loop.c 	\
    btstack_util.c		\
    hci_dump.c    		\
    l2cap_signaling.c   \
    hci_cmd.c 			\
    
# not used yet
#	btstack_em9304_spi_embedded.c \
#	btstack_stdin_embedded.c \
#	btstack_tlv_flash_bank.c \
#	btstack_uart_block_embedded.c \
#	hal_flash_bank_memory.c \
#   btstack_audio.c \
#   btstack_audio_embedded.c \
#   btstack_resample.c \
#   btstack_ring_buffer.c \
#   btstack_slip.c \
#   hci_transport_em9304_spi.c \
#   hci_transport_h4.c \
#   hci_transport_h5.c \

FREERTOS = \
	btstack_run_loop_freertos.c \
#	btstack_uart_block_freertos.c \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

FREERTOS_OBJ_COVERAGE = $(addprefix build-coverage/,$(FREERTOS:.c=.o))
FREERTOS_OBJ_ASAN     = $(addprefix build-asan/,    $(FREERTOS:.c=.o))

all: build-coverage/embedded_test build-asan/embedded_test \
	 build-coverage/run_loop_base_test build-asan/run_loop_base_test \
	 build-coverage/btstack_util_test build-asan/btstack_util_test \
	 build-coverage/l2cap_le_signaling_test build-asan/l2cap_le_signaling_test \
	 build-coverage/hci_cmd_test build-asan/hci_cmd_test \
	 build-coverage/freertos_test build-asan/freertos_test \

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@


build-coverage/embedded_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_run_loop_embedded.o build-coverage/embedded_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/embedded_test: ${COMMON_OBJ_ASAN} build-asan/embedded_test.o build-asan/btstack_run_loop_embedded.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


build-coverage/freertos_test: ${COMMON_OBJ_COVERAGE} ${FREERTOS_OBJS} build-coverage/freertos_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/freertos_test: ${COMMON_OBJ_ASAN} ${FREERTOS_OBJS} build-asan/freertos_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


build-coverage/run_loop_base_test: ${COMMON_OBJ_COVERAGE} build-coverage/run_loop_base_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/run_loop_base_test: ${COMMON_OBJ_ASAN} build-asan/run_loop_base_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


build-coverage/btstack_util_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_util_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/btstack_util_test: ${COMMON_OBJ_ASAN} build-asan/btstack_util_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


build-coverage/l2cap_le_signaling_test: ${COMMON_OBJ_COVERAGE} build-coverage/l2cap_le_signaling_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/l2cap_le_signaling_test: ${COMMON_OBJ_ASAN} build-asan/l2cap_le_signaling_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


build-coverage/hci_cmd_test: ${COMMON_OBJ_COVERAGE} build-coverage/hci_cmd_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/hci_cmd_test: ${COMMON_OBJ_ASAN} build-asan/hci_cmd_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/embedded_test
	build-asan/freertos_test
	build-asan/run_loop_base_test
	build-asan/btstack_util_test
	build-asan/l2cap_le_signaling_test
	build-asan/hci_cmd_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/embedded_test
	build-coverage/freertos_test
	build-coverage/run_loop_base_test
	build-coverage/btstack_util_test
	build-coverage/l2cap_le_signaling_test
	build-coverage/hci_cmd_test

clean:
	rm -rf build-coverage build-asan *.dSYM
