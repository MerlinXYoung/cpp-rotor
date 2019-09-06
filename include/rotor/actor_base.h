#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "messages.hpp"
#include "behavior.h"
#include <unordered_map>
#include "state.h"
#include <list>

namespace rotor {

struct supervisor_t;
struct system_context_t;
struct handler_base_t;

/** \brief intrusive pointer for handler */
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

/** \struct actor_base_t
 *  \brief universal primitives of concurrent computation
 *
 *  The class is base class for user-defined actors. It is expected that
 * actors will react on incoming messages (e.g. by changing internal
 * /private state) or send (other) messages to other actors, or do
 * some side-effects (I/O, etc.).
 *
 * Message passing interface is asynchronous, they are send to {@link supervisor_t}.
 *
 * Every actor belong to some {@link supervisor_t}, which "injects" the thread-safe
 * execution context, in a sense, that the actor can call it's own methods as well
 * as supervirors without any need of synchonization.
 *
 * All actor methods are thread-unsafe, i.e. should not be called with except of
 * it's own supervisor. Communication with actor should be performed via messages.
 *
 */
struct actor_base_t : public arc_base_t<actor_base_t> {
    /** \struct subscription_point_t
     *  \brief pair of {@link handler_base_t} linked to particular {@link address_t}
     */
    struct subscription_point_t {
        /** \brief intrusive pointer to messages' handler */
        handler_ptr_t handler;
        /** \brief intrusive pointer to address */
        address_ptr_t address;
    };

    /** \brief alias to the list of {@link subscription_point_t} */
    using subscription_points_t = std::list<subscription_point_t>;

    /** \brief constructs actor and links it's supervisor
     *
     * An actor cannot outlive it's supervisor.
     *
     * Sets internal actor state to `NEW`
     *
     */
    actor_base_t(supervisor_t &supervisor_);
    virtual ~actor_base_t();

    /** \brief early actor initialization (pre-initialization)
     *
     * Actor address is created.
     *
     * Actor performs subsscription on all major methods, defined by
     * `rotor` framework; sets internal actor state to `INITIALIZING`.
     *
     */
    virtual void do_initialize(system_context_t *ctx) noexcept;

    /** \brief convenient method to send actor's shutdown request to it's supervisor */
    virtual void do_shutdown() noexcept;

    /** \brief creates actor's address (by delegating the call to supervisor */
    virtual address_ptr_t create_address() noexcept;

    /** \brief returns actor's address (intrusive pointer) */
    inline address_ptr_t get_address() const noexcept { return address; }

    /** \brief returns actor's supervisor */
    inline supervisor_t &get_supervisor() const noexcept { return supervisor; }

    /** \brief returns actor's state */
    inline state_t &get_state() noexcept { return state; }

    /** \brief returns actor's subscription points */
    inline subscription_points_t &get_subscription_points() noexcept { return points; }

    /** \brief finishes actor's initialization.
     *
     * Sets internal actor state to `INITIALIZED` and sends initialization
     * confirmatin to it's actor.
     *
     * The method can be overriden in derived classes to release acquire
     * resources, i.e. (asynchronously) resolve host names. In that case
     * it is possible to suspend the initialization message and call
     * `on_initialize` only when an actor will be ready.
     *
     */
    virtual void on_initialize(message::init_request_t &) noexcept;

    /** \brief start confirmation from supervisor
     *
     * Sets internal actor state to `OPERATIONAL`.
     *
     */
    virtual void on_start(message_t<payload::start_actor_t> &) noexcept;

    /** \brief triggers actor's shutdown behavior
     *
     * If actor will not reply with shutdown confirmation, it's
     * supervisor will trigger appropriate behavior event.
     *
     */
    virtual void on_shutdown(message::shutdown_request_t &) noexcept;

    /** \brief initiates actor's shutdown
     *
     * If there is a supervisor, the message is forwarded to it to
     * send shutdown request
     *
     */
    virtual void on_shutdown_trigger(message::shutdown_trigger_t &) noexcept;

    /** \brief records subsciption point */
    virtual void on_subscription(message_t<payload::subscription_confirmation_t> &) noexcept;

    /** \brief forgets the subscription point
     *
     * If there is no more subscription points and actor is `SHUTTING_DOWN`,
     * then internal state is changed to `SHUTTED_DOWN` and `confirm_shutdown`
     * method is invoked to notify supervisor.
     *
     */
    virtual void on_unsubscription(message_t<payload::unsubscription_confirmation_t> &) noexcept;

    /** \brief forgets the subscription point for external address
     *
     * The {@link payload::commit_unsubscription_t} is sent to the external {@link supervisor_t}
     *  after removing the subscription .
     *
     */
    virtual void on_external_unsubscription(message_t<payload::external_unsubscription_t> &) noexcept;

    /** \brief sends message to the destination address
     *
     * Internally it just constructs new message in supervisor's outbound queue.
     *
     */
    template <typename M, typename... Args> void send(const address_ptr_t &addr, Args &&... args);

    /** \brief returns request builder for destination address using the default actor address
     *
     * The `args` are forwarded for construction of the request. The request is not actually sent,
     * until `timeout` method of `request_builder` will be invoked.
     *
     * Supervisor will spawn timeout timer upon `timeout` method.
     */
    template <typename M, typename... Args>
    request_builder_t<M> request(const address_ptr_t &dest_addr, Args &&... args);

    /** \brief returns request builder for destination address using the specified address for reply
     *
     * It is assumed, that the specified address belongs to the actor.
     *
     * The method is useful, when a different behaviour is needed for the same
     * message responce types. It serves at some extend as virtual dispatching within
     * the actor.
     *
     * See the description of `request` method.
     *
     */
    template <typename M, typename... Args>
    request_builder_t<M> request_via(const address_ptr_t &dest_addr, const address_ptr_t &reply_addr, Args &&... args);

    /** \brief convenient method for constructing and sending responce to a request
     *
     * `args` are forwarded to responce payload constuction
     */
    template <typename Request, typename... Args> void reply_to(Request &message, Args &&... args);

    /** \brief convenient method for constructing and sending error responce to a request */
    template <typename Request, typename... Args> void reply_with_error(Request &message, const std::error_code &ec);

    /** \brief subscribes actor's handler to process messages on the specified address */
    template <typename Handler> handler_ptr_t subscribe(Handler &&h, address_ptr_t &addr) noexcept;

    /** \brief subscribes actor's handler to process messages on the actor's address */
    template <typename Handler> handler_ptr_t subscribe(Handler &&h) noexcept;

    /** \brief unsubscribes actor's handler from process messages on the specified address */
    template <typename Handler> void unsubscribe(Handler &&h, address_ptr_t &addr) noexcept;

    /** \brief unsubscribes actor's handler from process messages on the actor's address */
    template <typename Handler> void unsubscribe(Handler &&h) noexcept;

  protected:
    /** \brief constructs actor's behaviour on early stage */
    virtual actor_behavior_t *create_behaviour() noexcept;

    /** \brief removes the subscription point */
    virtual void remove_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;

    /** \brief starts initialization
     *
     * Some resources might be acquired synchronously, if needed. If resources need
     * to be acquired asynchronously this method should be overriden, and
     * invoked only after resources acquisition.
     *
     * In internals it forwards initialization sequence to the behaviour.
     *
     */
    virtual void init_start() noexcept;

    /** \brief finializes initialization  */
    virtual void init_finish() noexcept;

    /** \brief strart releasing acquired resources
     *
     * The method can be overriden in derived classes to initiate the
     * release of resources, i.e. (asynchronously) close all opened
     * sockets before confirm shutdown to supervisor.
     *
     * It actually forwards shutdown for the behavior
     *
     */
    virtual void shutdown_start() noexcept;

    /** \brief finalize shutdown, release aquired resources
     *
     * This is the last action in the shutdown sequence.
     * No further methods will be invoked.
     *
     */
    virtual void shutdown_finish() noexcept;

    friend struct supervisor_t;

    /** \brief actor's execution / infrastructure context */
    supervisor_t &supervisor;

    /** \brief current actor state */
    state_t state;

    /** \brief actor's behavior, used for runtime customization of actors's
     * behavioural aspects
     */
    actor_behavior_t *behavior;

    /** \brief actor address */
    address_ptr_t address;

    /** \brief recorded subscription points (i.e. handler/address pairs) */
    subscription_points_t points;

    /** \brief suspended init request message */
    intrusive_ptr_t<message::init_request_t> init_request;

    /** \brief suspended shutdown request message */
    intrusive_ptr_t<message::shutdown_request_t> shutdown_request;

    friend struct actor_behavior_t;
    friend struct supervisor_t;
    template <typename T> friend struct request_builder_t;
};

/** \brief intrusive pointer for actor*/
using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

} // namespace rotor
