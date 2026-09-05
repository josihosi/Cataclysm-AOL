#pragma once
#ifndef CATA_SRC_SEMANTIC_SURFACE_H
#define CATA_SRC_SEMANTIC_SURFACE_H

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct semantic_action_descriptor {
    std::string id;
    std::string stable_id;
    std::string label;
    bool enabled = true;
};

struct semantic_surface_descriptor {
    int schema_version = 1;
    std::string run_id;
    std::string surface_id;
    std::string frame_id;
    std::string kind;
    std::vector<std::string> breadcrumbs;
    std::map<std::string, std::string> payload;
    std::vector<semantic_action_descriptor> valid_actions;
};

struct semantic_action_request {
    std::string run_id;
    std::string surface_id;
    std::string frame_id;
    std::string request_id;
    std::string action_id;
    std::optional<std::string> stable_id;
    std::map<std::string, std::string> parameters;
};

struct semantic_action_receipt {
    std::string request_id;
    std::string requested_run_id;
    std::string requested_surface_id;
    std::string requested_frame_id;
    std::string consuming_surface_id;
    std::string consuming_frame_id;
    std::string action_id;
    bool accepted = false;
    std::string rejection_reason;
    std::string resulting_frame_id;
};

struct semantic_action_dispatch_result {
    bool accepted = false;
    std::string rejection_reason;
    std::string resulting_frame_id;
    // A native owner may return only to construct its actual successor owner.
    // In that case, do not publish the transient parent frame between scopes.
    bool await_child_successor = false;
    // Most legacy owners bind an accepted receipt to their next native frame.
    // A dialogue action can instead enter a native modal immediately; its
    // receipt must be observable before that modal returns control.
    bool defer_receipt_to_successor = true;
};

using semantic_action_consumer = std::function<semantic_action_dispatch_result(
            const semantic_action_request &request )>;

class semantic_surface_manager
{
    public:
        explicit semantic_surface_manager( std::string run_id );

        const std::string &run_id() const;
        std::string push( const std::string &kind, const std::string &breadcrumb,
                          std::map<std::string, std::string> payload = {},
                          std::vector<semantic_action_descriptor> valid_actions = {},
                          semantic_action_consumer consumer = {} );
        bool publish( const std::string &surface_id,
                      std::map<std::string, std::string> payload,
                      std::vector<semantic_action_descriptor> valid_actions );
        bool pop( const std::string &surface_id );
        bool is_top( const std::string &surface_id ) const;
        // A native owner that is about to recreate itself after an input-owning
        // child returns can keep its old descriptor private while it unwinds.
        // The next explicit scope push publishes the recreated owner; this is
        // intentionally not a general child-suppression policy.
        bool withhold_parent_authority_until_recreated( const std::string &surface_id );
        const std::optional<semantic_surface_descriptor> &top() const;
        std::vector<semantic_surface_descriptor> stack() const;
        // Enqueue a renderer-neutral request.  The top native scope alone
        // consumes it, so this never creates an input event or key press.
        bool submit_request( semantic_action_request request );
        // Read complete, run-local transport records.  The caller must still
        // ask the current top scope to consume a queued request.
        bool poll_request_transport();
        bool has_pending_request() const;
        bool consume_top_request();
        bool queue_native_intent( const semantic_action_request &request, const std::string &intent );
        bool take_native_intent( const std::string &intent );
        void mark_transport_wake();
        bool take_transport_wake();
        semantic_action_receipt reject_request( const semantic_action_request &request );
        void set_descriptor_observer(
            std::function<void( const semantic_surface_descriptor & )> observer );
        void set_receipt_observer(
            std::function<void( const semantic_action_receipt & )> observer );

    private:
        struct surface_state {
            semantic_surface_descriptor descriptor;
            semantic_action_consumer consumer;
        };

        std::string new_surface_id();
        std::string new_frame_id();
        void republish_top();

        std::string run_id_;
        std::size_t next_surface_id_ = 0;
        std::size_t next_frame_id_ = 0;
        std::vector<surface_state> stack_;
        std::optional<semantic_surface_descriptor> top_;
        std::vector<semantic_action_request> pending_requests_;
        std::map<std::string, semantic_action_receipt> completed_requests_;
        std::optional<semantic_action_receipt> pending_accepted_receipt_;
        // A child-owned receipt must not resolve when its current owner merely
        // redraws after choosing.  Keep the owning surface identity until a
        // distinct successor surface is actually published.
        std::optional<std::string> pending_receipt_child_source_surface_id_;
        // The item-use path closes its old menu after a child (direction or a
        // confirmation) returns, then enters the same native menu loop anew.
        // Never expose the old menu or World during that handoff.
        std::set<std::string> withheld_parent_surface_ids_;
        bool suppress_parent_republish_ = false;
        std::size_t request_transport_offset_ = 0;
        bool transport_wake_pending_ = false;
        std::optional<std::pair<std::string, std::string>> native_intent_;
        std::function<void( const semantic_surface_descriptor & )> descriptor_observer_;
        std::function<void( const semantic_action_receipt & )> receipt_observer_;
};

// Binds a manager to the game thread while a native owner may open nested
// semantic surfaces.  The binding is deliberately scoped so unrelated input
// loops cannot inherit a stale parent surface.
class semantic_surface_manager_session
{
    public:
        explicit semantic_surface_manager_session( semantic_surface_manager &manager );
        ~semantic_surface_manager_session();

        semantic_surface_manager_session( const semantic_surface_manager_session & ) = delete;
        semantic_surface_manager_session &operator=( const semantic_surface_manager_session & ) = delete;

    private:
        semantic_surface_manager *previous_ = nullptr;
};

semantic_surface_manager *active_semantic_surface_manager();

// The harness reuses a run-bound manager across native turns so deferred
// activity owners can publish the successor selected by a World action.
bool openclaw_harness_semantic_session_active();
semantic_surface_manager &openclaw_harness_semantic_surface_manager();

// Input backends call this while blocked.  It only reads transport and wakes
// the active input context; it never consumes a request or constructs input.
bool poll_active_semantic_surface_request();
bool take_active_semantic_surface_wake();

class semantic_surface_scope
{
    public:
        semantic_surface_scope( semantic_surface_manager &manager, const std::string &kind,
                                const std::string &breadcrumb,
                                std::map<std::string, std::string> payload = {},
                                std::vector<semantic_action_descriptor> valid_actions = {},
                                semantic_action_consumer consumer = {} );
        ~semantic_surface_scope();

        semantic_surface_scope( const semantic_surface_scope & ) = delete;
        semantic_surface_scope &operator=( const semantic_surface_scope & ) = delete;
        semantic_surface_scope( semantic_surface_scope &&other ) noexcept;
        semantic_surface_scope &operator=( semantic_surface_scope &&other ) noexcept;

        const std::string &surface_id() const;
        bool publish( std::map<std::string, std::string> payload,
                      std::vector<semantic_action_descriptor> valid_actions );
        bool consume_request();

    private:
        void release();

        semantic_surface_manager *manager_ = nullptr;
        std::string surface_id_;
};

#endif // CATA_SRC_SEMANTIC_SURFACE_H
