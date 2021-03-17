/*
 * Copyright (C) 2021 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/ssh_session.h>
#include <shared/base_virtual_machine.h>

#include <gmock/gmock.h>

#include "dummy_ssh_key_provider.h"
#include "mock_ssh.h"

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace multipass
{
namespace test
{
struct StubBaseVirtualMachine : public mp::BaseVirtualMachine
{
    StubBaseVirtualMachine() : mp::BaseVirtualMachine("stub")
    {
    }

    void stop()
    {
    }

    void start()
    {
    }

    void shutdown()
    {
    }

    void suspend()
    {
    }

    mp::VirtualMachine::State current_state()
    {
        return mp::VirtualMachine::State::running;
    }

    int ssh_port()
    {
        return 42;
    }

    std::string ssh_hostname(std::chrono::milliseconds timeout)
    {
        return "localhost";
    }

    std::string ssh_username()
    {
        return "ubuntu";
    }

    std::string management_ipv4()
    {
        return "1.2.3.4";
    }

    std::string ipv6()
    {
        return "";
    }

    void wait_until_ssh_up(std::chrono::milliseconds timeout)
    {
    }

    void ensure_vm_is_running()
    {
    }

    void update_state()
    {
    }
};
} // namespace test
} // namespace multipass

namespace
{
struct BaseVM : public Test
{
    BaseVM()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
        request_exec.returnValue(SSH_OK);
        userauth_publickey.returnValue(SSH_OK);
        channel_is_closed.returnValue(0);
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
    decltype(MOCK(ssh_channel_request_exec)) request_exec{MOCK(ssh_channel_request_exec)};
    decltype(MOCK(ssh_userauth_publickey)) userauth_publickey{MOCK(ssh_userauth_publickey)};
    decltype(MOCK(ssh_channel_is_closed)) channel_is_closed{MOCK(ssh_channel_is_closed)};
    const mpt::DummyKeyProvider key_provider{"keeper of the seven keys"};
};

struct IpExecution : public BaseVM, public WithParamInterface<std::tuple<int, std::string, std::vector<std::string>>>
{
};
} // namespace

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_opening_a_session)
{
    mpt::StubBaseVirtualMachine base_vm;

    REPLACE(ssh_new, []() { return nullptr; }); // This makes SSH throw when opening a new session.
    EXPECT_THROW(mp::SSHSession("theanswertoeverything", 42), mp::SSHException); // Test that it indeed does.

    auto ipv4_count = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ipv4_count.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_executing)
{
    mpt::StubBaseVirtualMachine base_vm;

    // Make SSH throw when trying to execute something.
    request_exec.returnValue(SSH_ERROR);

    // Check that it indeed throws at execution.
    mp::SSHSession session{"host", 42};
    EXPECT_THROW(session.exec("dummy"), mp::SSHException);

    auto ipv4_count = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ipv4_count.size(), 0u);
}

TEST_P(IpExecution, get_all_ipv4_works_when_ssh_works)
{
    mpt::StubBaseVirtualMachine base_vm;

    auto [ip_exit_status, ip_output, expected_ips] = GetParam();
    auto remaining = ip_output.size();

    REPLACE(ssh_channel_get_exit_status, [](auto...) { return SSH_OK; });

    ssh_channel_callbacks callbacks{nullptr};
    auto add_channel_cbs = [&callbacks](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    auto event_dopoll = [&callbacks, &ip_exit_status](ssh_event, int timeout) {
        EXPECT_TRUE(callbacks);
        callbacks->channel_exit_status_function(nullptr, nullptr, ip_exit_status, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);

    auto channel_read = [&ip_output, &remaining](ssh_channel, void* dest, uint32_t count, int is_stderr, int) {
        const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
        const auto begin = ip_output.begin() + ip_output.size() - remaining;
        std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list, expected_ips);
}

INSTANTIATE_TEST_SUITE_P(BaseVM, IpExecution,
                         Values(std::make_tuple(0, "eth0             UP             192.168.2.168/24 \n",
                                                std::vector<std::string>{"192.168.2.168"}),
                                std::make_tuple(0,
                                                "wlp4s0           UP             192.168.2.8/24 \n"
                                                "virbr0           DOWN           192.168.3.1/24 \n"
                                                "tun0             UNKNOWN        10.172.66.5/18 \n",
                                                std::vector<std::string>{"192.168.2.8", "192.168.3.1", "10.172.66.5"}),
                                std::make_tuple(0, "", std::vector<std::string>{}),
                                std::make_tuple(127, "ip: command not found\n", std::vector<std::string>{})));
