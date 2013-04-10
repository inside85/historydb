#ifndef HISTORY_SRC_LIB_FEATURE_H
#define HISTORY_SRC_LIB_FEATURE_H

#include <map>
#include <elliptics/cppdef.h>

#include "historydb/iprovider.h"

/*namespace ioremap { namespace elliptics {
	class file_logger;
	class node;
	class session;
	typedef read_result;
}}*/

struct dnet_id;

namespace history {

	struct activity;
	class keys_size_cache;

	class features: public iprovider
	{
	public:
		features(ioremap::elliptics::file_logger& log, ioremap::elliptics::node& node, const std::vector<int>& groups, const uint32_t& min_writes, keys_size_cache& keys_cache);

		virtual void set_session_parameters(const std::vector<int>&, uint32_t) {}

		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string());
		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool log_written, bool statistics_updated)> func, const std::string& key = std::string());

		virtual void repartition_activity(const std::string& key, uint32_t parts);
		virtual void repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t parts);
		virtual void repartition_activity(uint64_t time, uint32_t parts);
		virtual void repartition_activity(uint64_t time, const std::string& new_key, uint32_t parts);

		virtual std::list<std::vector<char>> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time);

		virtual std::map<std::string, uint32_t> get_active_users(uint64_t time);
		virtual std::map<std::string, uint32_t> get_active_users(const std::string& key);

		virtual void for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(uint64_t time, void* data, uint32_t size)> func);

		virtual void for_active_users(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func);
		virtual void for_active_users(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func);

		void set_self(std::shared_ptr<features> self) { m_self = self; }

	private:
		features(const features&);
		features &operator =(const features &);

		/* Add data to the daily user log
			user - name of user
			time - timestamp of log
			data - pointer to the log data
			size - size of log data
		*/
		ioremap::elliptics::async_write_result add_user_data(void* data, uint32_t size);

		/* Increments user activity statistics
			user -nane of user
			time - timestamp
		*/
		ioremap::elliptics::async_write_result increment_activity();

		/* Makes user log id from user name and timestamp
			user - name of user
			time - timestamp
		*/
		std::string make_user_key(uint64_t time);

		/* Makes general activity statistics key
		*/
		std::string make_key(uint64_t time);

		/* Makes chunk key from general key
			key - general activity statistics key
			chunk - number of chunk
		*/
		std::string make_chunk_key(const std::string& key, uint32_t chunk);

		/* Gets chunk data
			key - general activity statistics key
			chunk - number of chunk
			act - where chunk data should be data written
			checksum - where checksum should be written
			returns true if chunk has successfully read and has been written to act (and to checksum)
					otherwise returns false
		*/
		bool get_chunk(const std::string& key, uint32_t chunk, activity& act, dnet_id* checksum = NULL);

		/* Writes data to elliptics in specified sessio
			key - id of file where data should be written
			data - pointer to the data
			size - size of data.
		*/
		bool write_data(const std::string& key, void* data, uint32_t size);

		/* Generate random value in range [0, max)
			max - the upper limit of random range
		*/
		uint32_t rand(uint32_t max);

		/*Merges two chunks. res_chunk will contain result of merging
			res_chunk - chunk in which result will be written
			merge_chunk - chunk data from which will be added to res_chunk
		*/
		void merge(activity& res_chunk, const activity& merge_chunk) const;

		/* Gets full activity statistics for specified general tree
			key - general key
		*/
		activity get_activity(const std::string& key);

		void increment_activity_callback(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info& error);

		void add_user_data_callback(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info& error);

		bool get_user_logs_callback(std::list<std::vector<char>>& ret, uint64_t time, void* data, uint32_t size);

		ioremap::elliptics::data_pointer write_cas_callback(const ioremap::elliptics::data_pointer& data);

		ioremap::elliptics::file_logger&	m_log;
		ioremap::elliptics::node&			m_node;
		std::shared_ptr<features>			m_self;
		const std::vector<int>&				m_groups;
		const uint32_t&						m_min_writes;
		keys_size_cache&					m_keys_cache;

		std::string							m_user;
		std::string							m_activity_key;
		std::string							m_user_key;
		bool								m_log_written;
		bool								m_stat_updated;
		ioremap::elliptics::session			m_session;
		std::function<void(bool log_written, bool statistics_updated)>	m_add_user_activity_callback;
	};

} /* namespace history */

#endif // HISTORY_SRC_LIB_FEATURE_H
