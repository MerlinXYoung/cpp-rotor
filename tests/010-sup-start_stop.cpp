//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = rotor::test;

static std::uint32_t destroyed = 0;

struct sample_sup_t;

struct sample_sup_t : public rt::supervisor_test_t {
    using sup_base_t = rt::supervisor_test_t;
    std::uint32_t initialized;
    std::uint32_t init_invoked;
    std::uint32_t start_invoked;
    std::uint32_t shutdown_started;
    std::uint32_t shutdown_finished;
    std::uint32_t shutdown_conf_invoked;
    r::address_ptr_t shutdown_addr;

    explicit sample_sup_t(const rt::supervisor_config_test_t &config) : r::test::supervisor_test_t{nullptr, config} {
        initialized = 0;
        init_invoked = 0;
        start_invoked = 0;
        shutdown_started = 0;
        shutdown_finished = 0;
        shutdown_conf_invoked = 0;
    }

    ~sample_sup_t() override { ++destroyed; }

    void do_initialize(r::system_context_t *ctx) noexcept override {
        ++initialized;
        sup_base_t::do_initialize(ctx);
    }

    void init_start() noexcept override {
        ++init_invoked;
        sup_base_t::init_start();
    }

    virtual void shutdown_finish() noexcept override {
        ++shutdown_finished;
        rt::supervisor_test_t::shutdown_finish();
    }
    virtual void shutdown_start() noexcept override {
        ++shutdown_started;
        rt::supervisor_test_t::shutdown_start();
    }

    virtual void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        ++start_invoked;
        sup_base_t::on_start(msg);
    }
};

TEST_CASE("on_initialize, on_start, simple on_shutdown", "[supervisor]") {
    r::system_context_t *system_context = new r::system_context_t{};
    rt::supervisor_config_test_t config(r::pt::milliseconds{1}, nullptr);
    auto sup = system_context->create_supervisor<sample_sup_t>(config);

    REQUIRE(&sup->get_supervisor() == sup.get());
    REQUIRE(sup->initialized == 1);

    sup->do_process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->start_invoked == 1);
    REQUIRE(sup->shutdown_started == 0);
    REQUIRE(sup->shutdown_conf_invoked == 0);
    REQUIRE(sup->active_timers.size() == 0);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->shutdown_started == 1);
    REQUIRE(sup->shutdown_finished == 1);
    REQUIRE(sup->active_timers.size() == 0);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);

    REQUIRE(destroyed == 0);
    delete system_context;
    sup->shutdown_addr.reset();
    sup.reset();
    REQUIRE(destroyed == 1);
}
