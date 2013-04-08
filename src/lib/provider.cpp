#include "provider.h"

#include <boost/algorithm/string.hpp>

#include <vector>
#include <time.h>
#include <msgpack.hpp>
#include <iomanip>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "activity.h"

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {

std::shared_ptr<iprovider> create_provider(const char* server_addr, const int server_port, const int family, const char* log_file, const int log_level)
{
	return std::make_shared<provider>(server_addr, server_port, family, log_file, log_level);
}

int get_log_level(const char* log_level)
{
			if (boost::iequals(log_level,	"DATA"))	return DNET_LOG_DATA;
	else	if (boost::iequals(log_level,	"ERROR"))	return DNET_LOG_ERROR;
	else	if (boost::iequals(log_level,	"INFO"))	return DNET_LOG_INFO;
	else	if (boost::iequals(log_level,	"NOTICE"))	return DNET_LOG_NOTICE;
	else	if (boost::iequals(log_level,	"DEBUG"))	return DNET_LOG_DEBUG;
	return DNET_LOG_INFO;
}

provider::provider(const char* server_addr, const int server_port, const int family, const char* log_file, const int log_level)
: m_log(log_file, log_level)
, m_node(m_log)
{
	srand(time(NULL));

	m_node.add_remote(server_addr, server_port, family);	// Adds connection parameters to the node.

	LOG(DNET_LOG_INFO, "Created provider for %s:%d:%d\n", server_addr, server_port, family);
}

provider::~provider()
{
	LOG(DNET_LOG_INFO, "Destroyed provder\n");
}

void provider::set_session_parameters(const std::vector<int>& groups, uint32_t min_writes)
{
	m_groups = groups;
	m_min_writes = min_writes > m_groups.size() ? m_groups.size() : min_writes;

	LOG(DNET_LOG_INFO, "Session parameters:\n");
	for (auto it = m_groups.begin(), it_end = m_groups.end(); it != it_end; ++it)
		LOG(DNET_LOG_INFO, "group:  %d\n", *it);
	LOG(DNET_LOG_INFO, "minimum number of successfull writes: %d\n", min_writes);
}

void provider::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key)
{
	make_features()->add_user_activity(user, time, data, size, key);
}

void provider::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool log_writed, bool statistics_updated)> func, const std::string& key)
{
	make_features()->add_user_activity(user, time, data, size, func, key);
}

void provider::repartition_activity(const std::string& key, uint32_t chunks)
{
	make_features()->repartition_activity(key, chunks);
}

void provider::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks)
{
	make_features()->repartition_activity(old_key, new_key, chunks);
}

void provider::repartition_activity(uint64_t time, uint32_t chunks)
{
	make_features()->repartition_activity(time, chunks);
}

void provider::repartition_activity(uint64_t time, const std::string& new_key, uint32_t chunks)
{
	make_features()->repartition_activity(time, new_key, chunks);
}

std::list<std::vector<char>> provider::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time)
{
	return make_features()->get_user_logs(user, begin_time, end_time);
}

std::map<std::string, uint32_t> provider::get_active_users(uint64_t time)
{
	return make_features()->get_active_users(time);
}

std::map<std::string, uint32_t> provider::get_active_users(const std::string& key)
{
	return make_features()->get_active_users(key);
}

void provider::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func)
{
	make_features()->for_user_logs(user, begin_time, end_time, func);
}

void provider::for_active_users(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func)
{
	make_features()->for_active_users(time, func);
}

void provider::for_active_users(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func)
{
	make_features()->for_active_users(key, func);
}

std::shared_ptr<features> provider::make_features()
{
	auto f = std::make_shared<features>(m_log, m_node, m_groups, m_min_writes, m_keys_cache);
	f->set_self(f);

	return f;
}

} /* namespace history */
