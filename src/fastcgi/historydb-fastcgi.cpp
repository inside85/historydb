#include "historydb-fastcgi.h"
#include <iostream>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <fastcgi2/logger.h>
#include <fastcgi2/config.h>
#include <fastcgi2/stream.h>
#include <fastcgi2/request.h>
#include <fastcgi2/component_factory.h>

#include <historydb/iprovider.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define ADD_HANDLER(script, func) m_handlers.insert(std::make_pair(script, boost::bind(&handler::func, this, _1, _2)));

namespace history { namespace fcgi {

	handler::handler(fastcgi::ComponentContext* context)
	: fastcgi::Component(context)
	, m_logger(NULL)
	{
		init_handlers(); // Inits handlers map
	}

	handler::~handler()
	{}

	void handler::onLoad()
	{
		const auto xpath = context()->getComponentXPath();
		auto config = context()->getConfig();
		const std::string logger_component_name = config->asString(xpath + "/logger"); // get logger name
		m_logger = context()->findComponent<fastcgi::Logger>(logger_component_name); // get logger component

		if (!m_logger) {
			throw std::runtime_error("cannot get component " + logger_component_name);
		}

		m_logger->debug("HistoryDB handler loaded\n");

		const auto e_addr		= config->asString(xpath + "/elliptics_addr"); // gets elliptics address from config
		const auto e_port		= config->asInt(xpath + "/elliptics_port"); // gets elliptics port from config
		const auto e_family		= config->asInt(xpath + "/elliptics_family"); // gets elliptics network family from config
		const auto log_file		= config->asString(xpath + "/log_file"); // gets historydb log file path from config
		const auto log_level	= config->asString(xpath + "/log_level"); // gets historydb log level from config

		m_provider = history::create_provider(e_addr.c_str(), e_port, e_family, log_file.c_str(), history::get_log_level(log_level.c_str())); // creates historydb provider instance

		m_logger->debug("HistoryDB provider has been created: elliptics address: %s:%d:%d %s %s\n", e_addr.c_str(), e_port, e_family, log_file.c_str(), log_level.c_str());

		std::vector<std::string> subs;
		std::vector<int> groups;

		m_logger->debug("Setting elliptics groups:\n");
		config->subKeys(xpath + "/elliptics_group_", subs); // gets set of groups keys in config

		int group = 0;
		for (auto it = subs.begin(), itEnd = subs.end(); it != itEnd; ++it) {
			group = config->asInt(*it); // gets group number from config
			groups.push_back(group); // adds the group number to vector
			m_logger->debug("Added %d group\n", group);
		}

		m_provider->set_session_parameters(groups, groups.size()); // sets provider session parameters
	}

	void handler::onUnload()
	{
		m_logger->debug("Unloading HistoryDB handler\n");
		m_provider.reset(); // destroys provider
		m_logger->debug("HistoryDB provider has been destroyed\n");
	}

	void handler::handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		auto script_name = req->getScriptName();
		m_logger->debug("Handle request: URI:%s\n", script_name.c_str());
		auto it = m_handlers.find(script_name); // finds handler for the script
		if (it != m_handlers.end()) { // if handler has been found
			it->second(req, context); // call handler
			return;
		}

		handle_wrong_uri(req, context); // calls wrong uri handler
	}

	void handler::init_handlers()
	{
		ADD_HANDLER("/",					handle_root);
		ADD_HANDLER("/test",				handle_test);
		ADD_HANDLER("/add_activity",		handle_add_activity);
		ADD_HANDLER("/get_active_users",	handle_get_active_users);
		ADD_HANDLER("/get_user_logs",		handle_get_user_logs);
	}

	void handler::handle_root(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle root request\n");
		fastcgi::RequestStream stream(req);

		stream <<	"\
					<html> \n\
						<head> \n\
						<script src=\"//ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js\"></script> \n\
						</head> \n\
						<body>\n\
							<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Add activity</div>\n\
								<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n\
									<form name=\"add\" action=\"add_activity\" method=\"post\">\n\
										User name: <input type=\"text\" name=\"user\" value=\"WebUser1\"><br>\n\
										Timestamp: <input type=\"text\" name=\"timestamp\" value=\"\"><br>\n\
										Data: <input type=\"text\" name=\"data\" value=\"Some data\"><br>\n\
										Key: <input type=\"text\" name=\"key\" valude=\"\"><br>\n\
										<input type=\"submit\" value=\"Send\">\n\
									</form>\n\
								</div>\n\
							</div>\n\
							<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Get active users</div>\n\
								<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n\
									<form name=\"get_user\" action=\"get_active_users\" method=\"get\">\n\
										Timestamp: <input type=\"text\" name=\"timestamp\" value=\"\"><br>\n\
										Key: <input type=\"text\" name=\"key\" valude=\"\"><br>\n\
										<input type=\"submit\" value=\"Send\">\n\
									</form>\n\
								</div>\n\
							</div>\n\
							<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Get user logs</div>\n\
								<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n\
									<form name=\"get_logs\" action=\"get_user_logs\" method=\"get\">\n\
										User name: <input type=\"text\" name=\"user\" value=\"WebUser1\"><br>\n\
										Begin timestamp: <input type=\"text\" name=\"begin_time\" value=\"\"><br>\n\
										End timestamp: <input type=\"text\" name=\"end_time\" value=\"\"><br>\n\
										<input type=\"submit\" value=\"Send\">\n\
									</form>\n\
								</div>\n\
							</div>\n\
						</body> \n\
					</html>";
	}

	void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle request for unknown/unexpected uri:%s\n", req->getURI().c_str());
		req->setStatus(404); // Sets 404 status for respone - wrong uri code
	}

	void handler::handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle add activity request\n");
		fastcgi::RequestStream stream(req);

		if (!req->hasArg("data")) { // checks required parameter data
			m_logger->debug("Required paramenter 'data' is missing\n");
			req->setStatus(404);
			return;
		}

		if (!req->hasArg("user")) { // checks required parameter user
			m_logger->debug("Required parameter 'user' is missing\n");
			req->setStatus(404);
			return;
		}

		auto user = req->getArg("user"); // gets user parameter
		auto data = req->getArg("data"); // gets data parameter

		std::string key = std::string();
		if (req->hasArg("key")) // checks optional parameter key
			key = req->getArg("key"); // gets key parameter

		uint64_t tm = time(NULL);
		if (req->hasArg("timestamp")) // checks optional parameter timestamp
			tm = boost::lexical_cast<uint64_t>(req->getArg("timestamp")); // gets timestamp parameter

		m_logger->debug("Add user activity: (%s, %d, ..., %d, %s\n", user.c_str(), tm, data.size(), key.c_str());
		m_provider->add_user_activity(user, tm, data.c_str(), data.size(), key); // calls provider function to add user activity
	}

	void handler::handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle get active user request\n");
		fastcgi::RequestStream stream(req);
		std::string key;
		uint64_t timestamp = -1;

		if (req->hasArg("key")) // checks optional parameter key
			key = req->getArg("key"); // gets key parameter

		if (req->hasArg("timestamp")) // checks optional parameter timestamp
			timestamp = boost::lexical_cast<uint64_t>(req->getArg("timestamp")); // gets timestamp parameter

		if (key.empty() && timestamp == (uint64_t)-1) { // if key and timestamp aren't among the parameters
			m_logger->debug("Key and timestamp are missing\n");
			req->setStatus(404);
			return;
		}

		std::map<std::string, uint32_t> res;
		if (!key.empty()) { // if key has been submitted
			m_logger->debug("Gets active users by key: %s\n", key.c_str());
			res = m_provider->get_active_users(key); // gets active users by key
		}
		else { // if timestamp has been submitted
			m_logger->debug("Gets active users by timestamp: %d\n", timestamp);
			res = m_provider->get_active_users(timestamp); // gets active users by timestamp
		}

		rapidjson::Document d; // creates document for json serialization
		d.SetObject();

		for (auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) { // adds all active user with counters to json
			d.AddMember(it->first.c_str(), it->second, d.GetAllocator());
		}

		rapidjson::StringBuffer buffer; // creates string buffer for serialized json
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
		d.Accept(writer); // accepts writer by json document

		auto json = buffer.GetString();

		m_logger->debug("Result json: %s\n", json);

		stream << json; // write result json to fastcgi stream
	}

	void handler::handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handlle get user logs request\n");
		fastcgi::RequestStream stream(req);
		if (!req->hasArg("user")) { // checks required parameter user
			m_logger->debug("Required parameter 'user' is missing\n");
			req->setStatus(404);
			return;
		}

		if (!req->hasArg("begin_time")) { // checks required parameter begin_time
			m_logger->debug("Required parameter 'begin_time' is missing\n");
			req->setStatus(404);
			return;
		}

		if (!req->hasArg("end_time")) { // checks required parameter end_time
			m_logger->debug("Required parameter 'end_time' is missing\n");
			req->setStatus(404);
			return;
		}

		auto user = req->getArg("user"); // gets user parameter
		auto begin_time = boost::lexical_cast<uint64_t>(req->getArg("begin_time")); // gets begin_time parameter
		auto end_time = boost::lexical_cast<uint64_t>(req->getArg("end_time")); // gets end_time parameter

		m_logger->debug("Gets user logs for: (%s, %d, %d)\n", user.c_str(), begin_time, end_time);
		auto res = m_provider->get_user_logs(user, begin_time, end_time); // gets user logs from historydb library

		rapidjson::Document d; // creates json document
		d.SetObject();

		rapidjson::Value user_logs(rapidjson::kArrayType); // array value which will contain all user logs for specified time

		for (auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {
			rapidjson::Value vec(&it->front(), it->size(), d.GetAllocator()); // creates vector value for user one's day log
			user_logs.PushBack(vec, d.GetAllocator()); // adds daily logs to result array
		}

		d.AddMember("logs", user_logs, d.GetAllocator()); // adds logs array to json document

		rapidjson::StringBuffer buffer; // creates string buffer for serialized json
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
		d.Accept(writer); // accepts writer by json document

		auto json = buffer.GetString();

		m_logger->debug("Result json: %s\n", json);

		stream << json; // writes result json to fastcgi stream
	}

	void handler::handle_test(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle test request\n");
		req->setStatus(200); // sets 200 status for test request.
	}

	FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
		FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
	FCGIDAEMON_REGISTER_FACTORIES_END()

} } /* namespace history { namespace fastcgi */
