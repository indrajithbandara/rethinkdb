#include <boost/bind.hpp>

#include "arch/arch.hpp"
#include "buffer_cache/buffer_cache.hpp"
#include "unittest/server_test_helper.hpp"
#include "unittest/unittest_utils.hpp"
#include "serializer/config.hpp"
#include "serializer/log/log_serializer.hpp"
#include "serializer/translator.hpp"
#include "btree/slice.hpp"

namespace unittest {

const int server_test_helper_t::init_value = 0x12345678;
const int server_test_helper_t::changed_value = 0x87654321;

server_test_helper_t::server_test_helper_t()
    : serializer(NULL), thread_pool(new thread_pool_t(1, false)) { }
server_test_helper_t::~server_test_helper_t() {
    delete thread_pool;
}

void server_test_helper_t::run() {
    struct starter_t : public thread_message_t {
        server_test_helper_t *server_test;
        explicit starter_t(server_test_helper_t *_server_test) : server_test(_server_test) { }
        void on_thread_switch() {
            coro_t::spawn(boost::bind(&server_test_helper_t::setup_server_and_run_tests, server_test));
        }
    } starter(this);

    thread_pool->run(&starter);
}

void server_test_helper_t::setup_server_and_run_tests() {
    temp_file_t db_file("/tmp/rdb_unittest.XXXXXX");

    {
        standard_serializer_t::create(
            standard_serializer_t::dynamic_config_t(),
            standard_serializer_t::private_dynamic_config_t(db_file.name()),
            standard_serializer_t::static_config_t()
            );
        standard_serializer_t log_serializer(
            standard_serializer_t::dynamic_config_t(),
            standard_serializer_t::private_dynamic_config_t(db_file.name())
            );

        std::vector<standard_serializer_t *> serializers;
        serializers.push_back(&log_serializer);
        serializer_multiplexer_t::create(serializers, 1);
        serializer_multiplexer_t multiplexer(serializers);

        this->serializer = multiplexer.proxies[0];

        run_serializer_tests();
    }

    trace_call(thread_pool->shutdown);
}

void server_test_helper_t::run_serializer_tests() {
    mirrored_cache_static_config_t cache_static_cfg;
    cache_t::create(this->serializer, &cache_static_cfg);
    mirrored_cache_config_t cache_cfg;
    cache_cfg.flush_timer_ms = 1000000;
    cache_cfg.flush_dirty_size = 1000000000;
    cache_cfg.max_size = GIGABYTE;
    cache_t cache(this->serializer, &cache_cfg, 0);

    nap(200);   // to let patch_disk_storage do writeback.sync();

    run_tests(&cache);
}

// Static helper functions
buf_lock_t *server_test_helper_t::create(transaction_t *txn) {
    buf_lock_t *buffer = new buf_lock_t;
    buffer->allocate(txn);
    return buffer;
}

void server_test_helper_t::snap(transaction_t *txn) {
    txn->snapshot();
}

buf_lock_t * server_test_helper_t::acq(transaction_t *txn, block_id_t block_id, access_t mode) {
    txn->assert_thread();
    return new buf_lock_t(txn, block_id, mode);
}

void server_test_helper_t::change_value(buf_lock_t *buf, uint32_t value) {
    buf->set_data(const_cast<void *>(buf->get_data_read()), &value, sizeof(value));
}

uint32_t server_test_helper_t::get_value(buf_lock_t *buf) {
    return *reinterpret_cast<const uint32_t *>(buf->get_data_read());
}

buf_lock_t *server_test_helper_t::acq_check_if_blocks_until_buf_released(transaction_t *txn, buf_lock_t *already_acquired_block, access_t acquire_mode, bool do_release, bool *blocked) {
    acquiring_coro_t acq_coro(txn, already_acquired_block->get_block_id(), acquire_mode);

    coro_t::spawn(boost::bind(&acquiring_coro_t::run, &acq_coro));
    nap(500);
    *blocked = !acq_coro.signaled;

    if (do_release) {
        delete already_acquired_block;
        if (*blocked) {
            nap(500);
            rassert(acq_coro.signaled, "Buf release must have unblocked the coroutine trying to acquire the buf. May be a bug in the test.");
        }
    }

    return acq_coro.result;
}

void server_test_helper_t::create_two_blocks(transaction_t *txn, block_id_t *block_A, block_id_t *block_B) {
    buf_lock_t *buf_A = create(txn);
    buf_lock_t *buf_B = create(txn);
    *block_A = buf_A->get_block_id();
    *block_B = buf_B->get_block_id();
    change_value(buf_A, init_value);
    change_value(buf_B, init_value);
    delete buf_A;
    delete buf_B;
}

}  // namespace unittest
