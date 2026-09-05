#include "semantic_surface.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#include "json.h"

namespace
{
thread_local semantic_surface_manager *active_manager = nullptr;

bool same_actions( const std::vector<semantic_action_descriptor> &left,
                   const std::vector<semantic_action_descriptor> &right )
{
    if( left.size() != right.size() ) {
        return false;
    }
    for( std::size_t index = 0; index < left.size(); ++index ) {
        if( left[index].id != right[index].id || left[index].stable_id != right[index].stable_id ||
            left[index].label != right[index].label || left[index].enabled != right[index].enabled ) {
            return false;
        }
    }
    return true;
}
} // namespace

semantic_surface_manager::semantic_surface_manager( std::string run_id ) : run_id_( std::move( run_id ) )
{
}

const std::string &semantic_surface_manager::run_id() const
{
    return run_id_;
}

std::string semantic_surface_manager::push( const std::string &kind, const std::string &breadcrumb,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions,
        semantic_action_consumer consumer )
{
    bool recreating_withheld_owner = false;
    for( auto owner = withheld_parent_surface_ids_.begin();
         owner != withheld_parent_surface_ids_.end(); ) {
        const bool owner_is_gone = std::none_of( stack_.begin(), stack_.end(),
        [&owner]( const surface_state &surface ) {
            return surface.descriptor.surface_id == *owner;
        } );
        if( owner_is_gone ) {
            owner = withheld_parent_surface_ids_.erase( owner );
            recreating_withheld_owner = true;
        } else {
            ++owner;
        }
    }
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
    stack_.push_back( { std::move( descriptor ), std::move( consumer ) } );
    if( recreating_withheld_owner ) {
        suppress_parent_republish_ = false;
    }
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
    if( stack_.back().descriptor.payload == payload &&
        same_actions( stack_.back().descriptor.valid_actions, valid_actions ) ) {
        return true;
    }
    stack_.back().descriptor.payload = std::move( payload );
    stack_.back().descriptor.valid_actions = std::move( valid_actions );
    const bool withheld_owner_is_gone = std::any_of( withheld_parent_surface_ids_.begin(),
                                         withheld_parent_surface_ids_.end(), [this]( const std::string &owner ) {
        return std::none_of( stack_.begin(), stack_.end(), [&owner]( const surface_state &surface ) {
            return surface.descriptor.surface_id == owner;
        } );
    } );
    if( withheld_parent_surface_ids_.count( surface_id ) != 0 || withheld_owner_is_gone ) {
        return true;
    }
    republish_top();
    return true;
}

bool semantic_surface_manager::pop( const std::string &surface_id )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    const bool withheld_parent_is_next = stack_.size() > 1 &&
                                        withheld_parent_surface_ids_.count(
                                            stack_[stack_.size() - 2].descriptor.surface_id ) != 0;
    const bool popping_withheld_parent = withheld_parent_surface_ids_.count( surface_id ) != 0;
    if( withheld_parent_is_next || popping_withheld_parent ) {
        stack_.pop_back();
        // The native item-use path is still unwinding.  Neither the stale
        // parent nor the World scope owns a request until the native loop
        // recreates the parent, so leave the manager without an active
        // descriptor for that private transition.
        top_.reset();
        return true;
    }
    if( suppress_parent_republish_ ) {
        stack_.pop_back();
        suppress_parent_republish_ = false;
        if( stack_.empty() ) {
            top_.reset();
        } else {
            top_ = stack_.back().descriptor;
        }
        return true;
    }
    stack_.pop_back();
    republish_top();
    return true;
}

bool semantic_surface_manager::is_top( const std::string &surface_id ) const
{
    return !stack_.empty() && stack_.back().descriptor.surface_id == surface_id;
}

bool semantic_surface_manager::withhold_parent_authority_until_recreated(
    const std::string &surface_id )
{
    if( !is_top( surface_id ) ) {
        return false;
    }
    // A committed dialogue can open an item menu that also retires while
    // its own child runs. Track both native owners independently.
    return withheld_parent_surface_ids_.insert( surface_id ).second;
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

bool semantic_surface_manager::submit_request( semantic_action_request request )
{
    if( request.request_id.empty() ) {
        return false;
    }
    const auto completed = completed_requests_.find( request.request_id );
    if( completed != completed_requests_.end() ) {
        if( receipt_observer_ ) {
            receipt_observer_( completed->second );
        }
        return false;
    }
    for( const semantic_action_request &pending : pending_requests_ ) {
        if( pending.request_id == request.request_id ) {
            return false;
        }
    }
    pending_requests_.push_back( std::move( request ) );
    return true;
}

bool semantic_surface_manager::poll_request_transport()
{
    const char *const path_value = std::getenv( "OPENCLAW_HARNESS_SEMANTIC_REQUEST_PATH" );
    if( path_value == nullptr || path_value[0] == '\0' ) {
        return false;
    }

    std::ifstream stream( path_value );
    if( !stream ) {
        return false;
    }
    stream.seekg( 0, std::ios::end );
    const std::streamoff end = stream.tellg();
    if( end < 0 ) {
        return false;
    }
    if( static_cast<std::size_t>( end ) < request_transport_offset_ ) {
        request_transport_offset_ = 0;
    }
    stream.seekg( static_cast<std::streamoff>( request_transport_offset_ ) );

    bool received = false;
    std::string line;
    while( std::getline( stream, line ) ) {
        request_transport_offset_ += line.size() + 1;
        if( line.empty() ) {
            continue;
        }
        try {
            std::istringstream json_stream( line );
            TextJsonIn jsin( json_stream );
            TextJsonObject record = jsin.get_object();
            record.allow_omitted_members();
            semantic_action_request request;
            request.run_id = record.get_string( "run_id", "" );
            request.surface_id = record.get_string( "surface_id", "" );
            request.frame_id = record.get_string( "frame_id", "" );
            request.request_id = record.get_string( "request_id", "" );
            request.action_id = record.get_string( "action_id", "" );
            if( record.has_string( "stable_id" ) ) {
                request.stable_id = record.get_string( "stable_id" );
            }
            if( record.has_member( "parameters" ) ) {
                TextJsonObject parameters = record.get_object( "parameters" );
                for( const TextJsonMember &member : parameters ) {
                    request.parameters.emplace( member.name(), member.get_string() );
                }
            }
            // A duplicate can replay its recorded receipt immediately and
            // leaves no pending request.  It still has to wake the input
            // loop so a curses getch() cannot hide that receipt behind a
            // second blocking read.
            received = true;
            submit_request( std::move( request ) );
        } catch( const std::exception & ) {
            // A malformed transport record is inert.  It cannot be repaired
            // into a request or produce a gameplay state change.
        }
    }
    return received;
}

bool semantic_surface_manager::has_pending_request() const
{
    return !pending_requests_.empty();
}

bool semantic_surface_manager::queue_native_intent( const semantic_action_request &request,
        const std::string &intent )
{
    if( intent.empty() || native_intent_ || request.run_id != run_id_ || request.request_id.empty() ) {
        return false;
    }
    native_intent_ = std::make_pair( request.request_id, intent );
    return true;
}

bool semantic_surface_manager::take_native_intent( const std::string &intent )
{
    if( !native_intent_ || native_intent_->second != intent ) {
        return false;
    }
    native_intent_.reset();
    return true;
}

bool semantic_surface_manager::consume_top_request()
{
    if( pending_requests_.empty() || stack_.empty() ) {
        return false;
    }

    semantic_action_request request = std::move( pending_requests_.front() );
    pending_requests_.erase( pending_requests_.begin() );
    semantic_action_receipt receipt;
    bool receipt_deferred = false;
    receipt.request_id = request.request_id;
    receipt.requested_run_id = request.run_id;
    receipt.requested_surface_id = request.surface_id;
    receipt.requested_frame_id = request.frame_id;
    receipt.action_id = request.action_id;
    receipt.consuming_surface_id = top_->surface_id;
    receipt.consuming_frame_id = top_->frame_id;

    if( request.run_id != run_id_ ) {
        receipt.rejection_reason = "wrong_run";
    } else if( request.surface_id != top_->surface_id ) {
        receipt.rejection_reason = "wrong_surface";
    } else if( request.frame_id != top_->frame_id ) {
        receipt.rejection_reason = "stale_frame";
    } else {
        const auto action = std::find_if( top_->valid_actions.begin(), top_->valid_actions.end(),
        [&request]( const semantic_action_descriptor &candidate ) {
            return candidate.id == request.action_id && candidate.enabled;
        } );
        if( action == top_->valid_actions.end() ) {
            receipt.rejection_reason = "unadvertised_action";
        } else if( !stack_.back().consumer ) {
            receipt.rejection_reason = "no_native_binding";
        } else {
            const semantic_action_dispatch_result result = stack_.back().consumer( request );
            receipt.accepted = result.accepted;
            receipt.rejection_reason = result.rejection_reason;
            receipt.resulting_frame_id = result.resulting_frame_id;
            if( receipt.accepted && receipt.resulting_frame_id.empty() &&
                result.defer_receipt_to_successor ) {
                // The consuming callback has selected a native action, but its
                // actual successor is published by the native action path.
                // Receipt that next top frame, never a temporary parent redraw.
                pending_accepted_receipt_ = receipt;
                suppress_parent_republish_ = result.await_child_successor;
                if( result.await_child_successor ) {
                    pending_receipt_child_source_surface_id_ = receipt.consuming_surface_id;
                } else {
                    pending_receipt_child_source_surface_id_.reset();
                }
                receipt_deferred = true;
            }
        }
    }
    completed_requests_.emplace( receipt.request_id, receipt );
    if( receipt_observer_ && !receipt_deferred ) {
        receipt_observer_( receipt );
    }
    return receipt.accepted;
}

bool semantic_surface_manager::take_transport_wake()
{
    const bool result = transport_wake_pending_;
    transport_wake_pending_ = false;
    return result;
}

void semantic_surface_manager::mark_transport_wake()
{
    transport_wake_pending_ = true;
}

semantic_action_receipt semantic_surface_manager::reject_request(
    const semantic_action_request &request )
{
    const auto previous = completed_requests_.find( request.request_id );
    if( previous != completed_requests_.end() ) {
        return previous->second;
    }
    semantic_action_receipt receipt;
    receipt.request_id = request.request_id;
    receipt.requested_run_id = request.run_id;
    receipt.requested_surface_id = request.surface_id;
    receipt.requested_frame_id = request.frame_id;
    receipt.action_id = request.action_id;
    if( !top_ ) {
        receipt.rejection_reason = "no_active_surface";
    } else {
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
    }
    if( receipt_observer_ ) {
        receipt_observer_( receipt );
    }
    completed_requests_.emplace( receipt.request_id, receipt );
    return receipt;
}

void semantic_surface_manager::set_descriptor_observer(
    std::function<void( const semantic_surface_descriptor & )> observer )
{
    descriptor_observer_ = std::move( observer );
}

void semantic_surface_manager::set_receipt_observer(
    std::function<void( const semantic_action_receipt & )> observer )
{
    receipt_observer_ = std::move( observer );
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
    if( descriptor_observer_ ) {
        descriptor_observer_( *top_ );
    }
    if( pending_accepted_receipt_ ) {
        if( pending_receipt_child_source_surface_id_ &&
            *pending_receipt_child_source_surface_id_ == top_->surface_id ) {
            return;
        }
        pending_accepted_receipt_->resulting_frame_id = top_->frame_id;
        completed_requests_[pending_accepted_receipt_->request_id] = *pending_accepted_receipt_;
        if( receipt_observer_ ) {
            receipt_observer_( *pending_accepted_receipt_ );
        }
        pending_accepted_receipt_.reset();
        pending_receipt_child_source_surface_id_.reset();
    }
}

semantic_surface_manager_session::semantic_surface_manager_session(
    semantic_surface_manager &manager ) : previous_( active_manager )
{
    active_manager = &manager;
}

semantic_surface_manager_session::~semantic_surface_manager_session()
{
    active_manager = previous_;
}

semantic_surface_manager *active_semantic_surface_manager()
{
    return active_manager;
}

bool poll_active_semantic_surface_request()
{
    if( active_manager == nullptr ) {
        return false;
    }
    if( !active_manager->poll_request_transport() ) {
        return false;
    }
    // A duplicate is already completed and therefore has no pending request,
    // but consuming its transport record replayed the exact receipt.  Wake
    // the input loop in either case so terminal input cannot block that
    // replay behind a physical read.
    active_manager->mark_transport_wake();
    return true;
}

bool take_active_semantic_surface_wake()
{
    return active_manager != nullptr && active_manager->take_transport_wake();
}

semantic_surface_scope::semantic_surface_scope( semantic_surface_manager &manager,
        const std::string &kind, const std::string &breadcrumb,
        std::map<std::string, std::string> payload,
        std::vector<semantic_action_descriptor> valid_actions,
        semantic_action_consumer consumer ) :
    manager_( &manager ), surface_id_( manager.push( kind, breadcrumb, std::move( payload ),
                                      std::move( valid_actions ), std::move( consumer ) ) )
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

bool semantic_surface_scope::consume_request()
{
    return manager_ && manager_->is_top( surface_id_ ) && manager_->consume_top_request();
}

void semantic_surface_scope::release()
{
    if( manager_ ) {
        manager_->pop( surface_id_ );
        manager_ = nullptr;
    }
}
