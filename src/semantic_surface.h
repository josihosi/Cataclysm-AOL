#pragma once
#ifndef CATA_SRC_SEMANTIC_SURFACE_H
#define CATA_SRC_SEMANTIC_SURFACE_H

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
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

class semantic_surface_manager
{
    public:
        explicit semantic_surface_manager( std::string run_id );

        const std::string &run_id() const;
        std::string push( const std::string &kind, const std::string &breadcrumb,
                          std::map<std::string, std::string> payload = {},
                          std::vector<semantic_action_descriptor> valid_actions = {} );
        bool publish( const std::string &surface_id,
                      std::map<std::string, std::string> payload,
                      std::vector<semantic_action_descriptor> valid_actions );
        bool pop( const std::string &surface_id );
        bool is_top( const std::string &surface_id ) const;
        const std::optional<semantic_surface_descriptor> &top() const;
        std::vector<semantic_surface_descriptor> stack() const;
        semantic_action_receipt reject_request( const semantic_action_request &request ) const;

    private:
        struct surface_state {
            semantic_surface_descriptor descriptor;
        };

        std::string new_surface_id();
        std::string new_frame_id();
        void republish_top();

        std::string run_id_;
        std::size_t next_surface_id_ = 0;
        std::size_t next_frame_id_ = 0;
        std::vector<surface_state> stack_;
        std::optional<semantic_surface_descriptor> top_;
};

class semantic_surface_scope
{
    public:
        semantic_surface_scope( semantic_surface_manager &manager, const std::string &kind,
                                const std::string &breadcrumb,
                                std::map<std::string, std::string> payload = {},
                                std::vector<semantic_action_descriptor> valid_actions = {} );
        ~semantic_surface_scope();

        semantic_surface_scope( const semantic_surface_scope & ) = delete;
        semantic_surface_scope &operator=( const semantic_surface_scope & ) = delete;
        semantic_surface_scope( semantic_surface_scope &&other ) noexcept;
        semantic_surface_scope &operator=( semantic_surface_scope &&other ) noexcept;

        const std::string &surface_id() const;
        bool publish( std::map<std::string, std::string> payload,
                      std::vector<semantic_action_descriptor> valid_actions );

    private:
        void release();

        semantic_surface_manager *manager_ = nullptr;
        std::string surface_id_;
};

#endif // CATA_SRC_SEMANTIC_SURFACE_H
