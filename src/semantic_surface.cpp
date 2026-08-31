#include "semantic_surface.h"

#include <utility>

semantic_surface_manager::semantic_surface_manager( std::string run_id ) : run_id_( std::move( run_id ) )
{
}

const std::string &semantic_surface_manager::run_id() const
{
    return run_id_;
}

std::string semantic_surface_manager::push( const std::string &kind, const std::string &breadcrumb,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions )
{
    semantic_surface_descriptor descriptor;
    descriptor.run_id = run_id_;
    descriptor.surface_id = new_surface_id();
    descriptor.kind = kind;
    descriptor.payload = std::move( payload );
    descriptor.valid_actions = std::move( valid_actions );
    for( const surface_state &parent : stack_ ) {
        descriptor.breadcrumbs.push_back( parent.descriptor.breadcrumbs.back() );
    }
    descriptor.breadcrumbs.push_back( breadcrumb );
    stack_.push_back( { std::move( descriptor ) } );
    republish_top();
    return stack_.back().descriptor.surface_id;
}

bool semantic_surface_manager::publish( const std::string &surface_id,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    stack_.back().descriptor.payload = std::move( payload );
    stack_.back().descriptor.valid_actions = std::move( valid_actions );
    republish_top();
    return true;
}

bool semantic_surface_manager::pop( const std::string &surface_id )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    stack_.pop_back();
    republish_top();
    return true;
}

bool semantic_surface_manager::is_top( const std::string &surface_id ) const
{
    return !stack_.empty() && stack_.back().descriptor.surface_id == surface_id;
}

const std::optional<semantic_surface_descriptor> &semantic_surface_manager::top() const
{
    return top_;
}

std::vector<semantic_surface_descriptor> semantic_surface_manager::stack() const
{
    std::vector<semantic_surface_descriptor> result;
    result.reserve( stack_.size() );
    for( const surface_state &surface : stack_ ) {
        result.push_back( surface.descriptor );
    }
    return result;
}

semantic_action_receipt semantic_surface_manager::reject_request(
    const semantic_action_request &request ) const
{
    semantic_action_receipt receipt;
    receipt.request_id = request.request_id;
    receipt.requested_run_id = request.run_id;
    receipt.requested_surface_id = request.surface_id;
    receipt.requested_frame_id = request.frame_id;
    receipt.action_id = request.action_id;
    if( !top_ ) {
        receipt.rejection_reason = "no_active_surface";
        return receipt;
    }
    receipt.consuming_surface_id = top_->surface_id;
    receipt.consuming_frame_id = top_->frame_id;
    if( request.run_id != run_id_ ) {
        receipt.rejection_reason = "wrong_run";
    } else if( request.surface_id != top_->surface_id ) {
        receipt.rejection_reason = "wrong_surface";
    } else if( request.frame_id != top_->frame_id ) {
        receipt.rejection_reason = "stale_frame";
    } else {
        receipt.rejection_reason = "no_native_binding";
    }
    return receipt;
}

std::string semantic_surface_manager::new_surface_id()
{
    return run_id_ + ":surface:" + std::to_string( ++next_surface_id_ );
}

std::string semantic_surface_manager::new_frame_id()
{
    return run_id_ + ":frame:" + std::to_string( ++next_frame_id_ );
}

void semantic_surface_manager::republish_top()
{
    if( stack_.empty() ) {
        top_.reset();
        return;
    }
    stack_.back().descriptor.frame_id = new_frame_id();
    top_ = stack_.back().descriptor;
}

semantic_surface_scope::semantic_surface_scope( semantic_surface_manager &manager,
        const std::string &kind, const std::string &breadcrumb,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions ) :
    manager_( &manager ), surface_id_( manager.push( kind, breadcrumb, std::move( payload ),
                                      std::move( valid_actions ) ) )
{
}

semantic_surface_scope::~semantic_surface_scope()
{
    release();
}

semantic_surface_scope::semantic_surface_scope( semantic_surface_scope &&other ) noexcept :
    manager_( other.manager_ ), surface_id_( std::move( other.surface_id_ ) )
{
    other.manager_ = nullptr;
}

semantic_surface_scope &semantic_surface_scope::operator=( semantic_surface_scope &&other ) noexcept
{
    if( this != &other ) {
        release();
        manager_ = other.manager_;
        surface_id_ = std::move( other.surface_id_ );
        other.manager_ = nullptr;
    }
    return *this;
}

const std::string &semantic_surface_scope::surface_id() const
{
    return surface_id_;
}

bool semantic_surface_scope::publish( std::map<std::string, std::string> payload,
                                      std::vector<semantic_action_descriptor> valid_actions )
{
    return manager_ && manager_->publish( surface_id_, std::move( payload ), std::move( valid_actions ) );
}

void semantic_surface_scope::release()
{
    if( manager_ ) {
        manager_->pop( surface_id_ );
        manager_ = nullptr;
    }
}
