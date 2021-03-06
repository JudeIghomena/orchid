/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_EVENT_HPP
#define ORCHID_EVENT_HPP

#include <cppcoro/async_manual_reset_event.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include "task.hpp"

namespace orc {

class Event {
  private:
    cppcoro::async_manual_reset_event ready_;

  public:
    operator bool() {
        return ready_.is_set();
    }

    void operator ()() {
        ready_.set();
    }

    // XXX: replace with operator co_await
    task<void> Wait() {
        co_await ready_;
        co_await Schedule();
    }
};

}

#endif//ORCHID_EVENT_HPP
