//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = r::test;

struct sample_actor_t  : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
};

struct observer_t: public r::actor_base_t {
    std::uint32_t event = 0;
    r::address_ptr_t observable;

    using r::actor_base_t::actor_base_t;

    void set_observable(r::address_ptr_t addr) { observable = std::move(addr); }

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&observer_t::on_sample_initialize, observable);
        subscribe(&observer_t::on_sample_start, observable);
        subscribe(&observer_t::on_sample_shutdown, observable);
    }

    void on_sample_initialize(r::message_t<r::payload::initialize_actor_t>&) noexcept {
        event += 1;
    }

    void on_sample_start(r::message_t<r::payload::start_actor_t> &) noexcept {
        event += 2;
    }

    void on_sample_shutdown(r::message_t<r::payload::shutdown_request_t> &) noexcept {
        event += 4;
    }

};


TEST_CASE("lifetime observer", "[actor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<rt::supervisor_test_t>(nullptr, nullptr);
    auto observer = sup->create_actor<observer_t>();
    auto sample_actor = sup->create_actor<sample_actor_t>();
    observer->set_observable(sample_actor->get_address());

    sup->do_start();
    sup->do_process();
    REQUIRE(observer->event == 3);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(observer->event == 7);

    REQUIRE(sup->get_state() == r::state_t::SHUTTED_DOWN);
    REQUIRE(sup->get_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(sup->get_subscription().size() == 0);
}
