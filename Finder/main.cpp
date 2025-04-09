#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <boost/locale.hpp>
#include <boost/beast.hpp>

#include "../Spider/ParcerINI.h"
#include "../Spider/database.h"
#include "finder.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Глобальная переменная - база данных

database DB;

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
	using beast::iequals;
	auto const ext = [&path]
		{
			auto const pos = path.rfind(".");
			if (pos == beast::string_view::npos)
				return beast::string_view{};
			return path.substr(pos);
		}();
	if (iequals(ext, ".htm"))  return "text/html";
	if (iequals(ext, ".html")) return "text/html";
	if (iequals(ext, ".php"))  return "text/html";
	if (iequals(ext, ".css"))  return "text/css";
	if (iequals(ext, ".txt"))  return "text/plain";
	if (iequals(ext, ".js"))   return "application/javascript";
	if (iequals(ext, ".json")) return "application/json";
	if (iequals(ext, ".xml"))  return "application/xml";
	if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
	if (iequals(ext, ".flv"))  return "video/x-flv";
	if (iequals(ext, ".png"))  return "image/png";
	if (iequals(ext, ".jpe"))  return "image/jpeg";
	if (iequals(ext, ".jpeg")) return "image/jpeg";
	if (iequals(ext, ".jpg"))  return "image/jpeg";
	if (iequals(ext, ".gif"))  return "image/gif";
	if (iequals(ext, ".bmp"))  return "image/bmp";
	if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
	if (iequals(ext, ".tiff")) return "image/tiff";
	if (iequals(ext, ".tif"))  return "image/tiff";
	if (iequals(ext, ".svg"))  return "image/svg+xml";
	if (iequals(ext, ".svgz")) return "image/svg+xml";
	return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
	beast::string_view base,
	beast::string_view path)
{
	if (base.empty())
		return std::string(path);
	std::string result(base);
#ifdef BOOST_MSVC
	char constexpr path_separator = '\\';
	if (result.back() == path_separator)
		result.resize(result.size() - 1);
	result.append(path.data(), path.size());
	for (auto& c : result)
		if (c == '/')
			c = path_separator;
#else
	char constexpr path_separator = '/';
	if (result.back() == path_separator)
		result.resize(result.size() - 1);
	result.append(path.data(), path.size());
#endif
	return result;
}

// Функция для декодирования URL
std::string url_decode(const std::string& in) {
	std::string out;
	boost::beast::string_view sv(in);
	out.reserve(sv.size());

	for (std::size_t i = 0; i < sv.size(); ++i) {
		if (sv[i] == '%' && i + 2 < sv.size() &&
			std::isxdigit(sv[i + 1]) && std::isxdigit(sv[i + 2])) {
			int hi = std::isdigit(sv[i + 1]) ? sv[i + 1] - '0' : std::tolower(sv[i + 1]) - 'a' + 10;
			int lo = std::isdigit(sv[i + 2]) ? sv[i + 2] - '0' : std::tolower(sv[i + 2]) - 'a' + 10;
			out.push_back((hi << 4) | lo);
			i += 2;
		}
		else if (sv[i] == '+') {
			out.push_back(' ');
		}
		else {
			out.push_back(sv[i]);
		}
	}
	//out = boost::locale::conv::between(out, "UTF-8", "ISO-8859-1");
	return out;
}


// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template <class Body, class Allocator>
http::message_generator
handle_request(
	beast::string_view doc_root,
	http::request<Body, http::basic_fields<Allocator>>&& req)
{
	// Returns a bad request response
	auto const bad_request =
		[&req](beast::string_view why)
		{
			http::response<http::string_body> res{ http::status::bad_request, req.version() };
			res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = std::string(why);
			res.prepare_payload();
			return res;
		};

	// Returns a not found response
	auto const not_found =
		[&req](beast::string_view target)
		{
			http::response<http::string_body> res{ http::status::not_found, req.version() };
			res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = "The resource '" + std::string(target) + "' was not found.";
			res.prepare_payload();
			return res;
		};

	// Returns a server error response
	auto const server_error =
		[&req](beast::string_view what)
		{
			http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
			res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(http::field::content_type, "text/html");
			res.keep_alive(req.keep_alive());
			res.body() = "An error occurred: '" + std::string(what) + "'";
			res.prepare_payload();
			return res;
		};

	// Make sure we can handle the method
	if (req.method() != http::verb::get &&
		req.method() != http::verb::head &&
		req.method() != http::verb::post
		)
		return bad_request("Unknown HTTP-method");

	// Respond to GET request
	if (req.method() == http::verb::get)
	{
		// Generate the HTML form
			// Generate the HTML form with UTF-8 encoding and centered form
		std::ostringstream body;
		body << "<!DOCTYPE html>\n"
			<< "<html>\n"
			<< "<head>\n"
			<< "    <meta charset=\"utf-8\">\n"
			<< "    <title>Поисковик</title>\n"
			<< "    <style>\n"
			<< "        .container {\n"
			<< "            display: flex;\n"
			<< "            flex-direction: column;\n"
			<< "            align-items: center;\n"
			<< "            height: 100vh;\n"
			<< "            justify-content: center;\n"
			<< "        }\n"
			<< "        form {\n"
			<< "            text-align: center;\n"
			<< "        }\n"
			<< "    </style>\n"
			<< "</head>\n"
			<< "<body>\n"
			<< "    <div class=\"container\">\n"
			<< "        <h1>Поисковик</h1>\n"
			<< "        <form action=\"/search\" method=\"POST\">\n"
			<< "            <label for=\"query\">Введите запрос:</label><br>\n"
			<< "            <input type=\"text\" id=\"query\" name=\"query\" placeholder=\"Введите запрос\"><br>\n"
			<< "            <input type=\"submit\" value=\"Найти\">\n"
			<< "        </form>\n"
			<< "    </div>\n"
			<< "</body>\n"
			<< "</html>\n";

		http::response<http::string_body> res{
			std::piecewise_construct,
			std::make_tuple(std::move(body.str())),
			std::make_tuple(http::status::ok, req.version()) };

		res.set(http::field::content_type, "text/html; charset=utf-8");
		res.content_length(body.str().size());
		res.keep_alive(req.keep_alive());

		return res;
	}

	// Respond to POST request
	if (req.method() == http::verb::post && req.target() == "/search")
	{
		// Получение данных из POST-запроса
		auto const body = req.body();

		// Выполнение поиска на основе полученных данных
		// Здесь предполагается, что результаты поиска хранятся в переменной searchResults
		std::string queryData = body; //"Полученные данные из запроса POST";


		// Почему-то в запросе лишний префикс - удалим
		std::string queryPrefix = "query=";
		if (body.substr(0, queryPrefix.size()) == queryPrefix) {
			// Удаляем префикс "query="
			queryData = body.substr(queryPrefix.size());
		}
		//Выполним перекодировку URL
		queryData = url_decode(queryData);
		std::vector<std::string> searchResults = finder(queryData, DB);
		std::cout << "Query Data: " << queryData << std::endl;
		// Формирование HTML-страницы с результатами поиска
		std::ostringstream responseBody;
		responseBody << "<!DOCTYPE html>\n"
			<< "<html>\n"
			<< "<head>\n"
			<< "    <meta charset=\"utf-8\">\n"
			<< "    <title>Результаты поиска</title>\n"
			<< "    <style>\n"
			<< "        body {\n"
			<< "            display: flex;\n"
			<< "            justify-content: center;\n"
			<< "            align-items: center;\n"
			<< "            height: 100vh;\n"
			<< "            margin: 0;\n"
			<< "        }\n"
			<< "        .container {\n"
			<< "            text-align: center;\n"
			<< "        }\n"
			<< "    </style>\n"
			<< "</head>\n"
			<< "<body>\n"
			<< "    <div class=\"container\">\n"
			<< "        <h1>Результаты поиска</h1>\n"
			<< "        <p>Результаты вашего поиска: " << queryData << "</p>\n"
			<< "        <div class=\"search-results\">\n";

		// Добавление формы для нового поиска
		responseBody << "        <form action=\"/search\" method=\"POST\">\n"
			<< "            <label for=\"query\">Введите запрос:</label><br>\n"
			<< "            <input type=\"text\" id=\"query\" name=\"query\" placeholder=\"Введите запрос\"><br>\n"
			<< "            <input type=\"submit\" value=\"Найти\">\n"
			<< "        </form>\n";

		// Добавление результатов поиска в HTML как ссылок
		unsigned int linkIter = 0;
		for (const auto& searchResultString : searchResults)
		{
			// Выведем результаты в виде кликабельных ссылок
			responseBody << "<p><a href=\"" << searchResultString << "\">" << searchResultString << "</a></p>\n";
			++linkIter;
			if (linkIter > 10) break;
		}
		if (searchResults.empty())
		{
			responseBody << "<p>" << "Ничего не найдено по запросу: " << queryData << "</p>\n";
		}

		responseBody << "        </div>\n"
			<< "    </div>\n"
			<< "</body>\n"
			<< "</html>\n";

		// Формирование HTTP-ответа с HTML-страницей
		http::response<http::string_body> res{
			std::piecewise_construct,
			std::make_tuple(std::move(responseBody.str())),
			std::make_tuple(http::status::ok, req.version()) };

		res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
		res.set(http::field::content_type, "text/html; charset=utf-8");
		res.content_length(responseBody.str().size());
		res.keep_alive(req.keep_alive());

		return res;
	}

}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
	std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
	beast::tcp_stream stream_;
	beast::flat_buffer buffer_;
	std::shared_ptr<std::string const> doc_root_;
	http::request<http::string_body> req_;

public:
	// Take ownership of the stream
	session(
		tcp::socket&& socket,
		std::shared_ptr<std::string const> const& doc_root)
		: stream_(std::move(socket))
		, doc_root_(doc_root)
	{
	}

	// Start the asynchronous operation
	void
		run()
	{
		// We need to be executing within a strand to perform async operations
		// on the I/O objects in this session. Although not strictly necessary
		// for single-threaded contexts, this example code is written to be
		// thread-safe by default.
		net::dispatch(stream_.get_executor(),
			beast::bind_front_handler(
				&session::do_read,
				shared_from_this()));
	}

	void
		do_read()
	{
		// Make the request empty before reading,
		// otherwise the operation behavior is undefined.
		req_ = {};

		// Set the timeout.
		stream_.expires_after(std::chrono::seconds(30));

		// Read a request
		http::async_read(stream_, buffer_, req_,
			beast::bind_front_handler(
				&session::on_read,
				shared_from_this()));
	}

	void
		on_read(
			beast::error_code ec,
			std::size_t bytes_transferred)
	{
		boost::ignore_unused(bytes_transferred);

		// This means they closed the connection
		if (ec == http::error::end_of_stream)
			return do_close();

		if (ec)
			return fail(ec, "read");

		// Send the response
		send_response(
			handle_request(*doc_root_, std::move(req_)));
	}

	void
		send_response(http::message_generator&& msg)
	{
		bool keep_alive = msg.keep_alive();

		// Write the response
		beast::async_write(
			stream_,
			std::move(msg),
			beast::bind_front_handler(
				&session::on_write, shared_from_this(), keep_alive));
	}

	void
		on_write(
			bool keep_alive,
			beast::error_code ec,
			std::size_t bytes_transferred)
	{
		boost::ignore_unused(bytes_transferred);

		if (ec)
			return fail(ec, "write");

		if (!keep_alive)
		{
			// This means we should close the connection, usually because
			// the response indicated the "Connection: close" semantic.
			return do_close();
		}

		// Read another request
		do_read();
	}

	void
		do_close()
	{
		// Send a TCP shutdown
		beast::error_code ec;
		stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

		// At this point the connection is closed gracefully
	}
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
	net::io_context& ioc_;
	tcp::acceptor acceptor_;
	std::shared_ptr<std::string const> doc_root_;

public:
	listener(
		net::io_context& ioc,
		tcp::endpoint endpoint,
		std::shared_ptr<std::string const> const& doc_root)
		: ioc_(ioc)
		, acceptor_(net::make_strand(ioc))
		, doc_root_(doc_root)
	{
		beast::error_code ec;

		// Open the acceptor
		acceptor_.open(endpoint.protocol(), ec);
		if (ec)
		{
			fail(ec, "open");
			return;
		}

		// Allow address reuse
		acceptor_.set_option(net::socket_base::reuse_address(true), ec);
		if (ec)
		{
			fail(ec, "set_option");
			return;
		}

		// Bind to the server address
		acceptor_.bind(endpoint, ec);
		if (ec)
		{
			fail(ec, "bind");
			return;
		}

		// Start listening for connections
		acceptor_.listen(
			net::socket_base::max_listen_connections, ec);
		if (ec)
		{
			fail(ec, "listen");
			return;
		}
	}

	// Start accepting incoming connections
	void
		run()
	{
		do_accept();
	}

private:
	void
		do_accept()
	{
		// The new connection gets its own strand
		acceptor_.async_accept(
			net::make_strand(ioc_),
			beast::bind_front_handler(
				&listener::on_accept,
				shared_from_this()));
	}

	void
		on_accept(beast::error_code ec, tcp::socket socket)
	{
		if (ec)
		{
			fail(ec, "accept");
			return; // To avoid infinite loop
		}
		else
		{
			// Create the session and run it
			std::make_shared<session>(
				std::move(socket),
				doc_root_)->run();
		}

		// Accept another connection
		do_accept();
	}
};

std::string DataBaseHostName;
std::string DataBaseName;
std::string DataBaseUserName;
std::string DataBasePassword;
int DataBasePort;


std::string FinderAddress;
int FinderPort;

int main(int argc, char* argv[])
{
	//setlocale(LC_ALL, "Russian");
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001); //UTF-8

	// Прочитаем конфигурацию в файле configuration.ini
	try {
		char buffer[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, buffer);
		std::string filePath = std::string(buffer) + "\\configuration.ini"; // определим путь к исполняемому файлу
		std::cout << filePath;
		ParcerINI parser = ParcerINI(filePath);

		DataBaseHostName = parser.get_value<std::string>("DataBase.HostName");
		DataBaseName = parser.get_value<std::string>("DataBase.DatabaseName");
		DataBaseUserName = parser.get_value<std::string>("DataBase.UserName");
		DataBasePassword = parser.get_value<std::string>("DataBase.Password");
		DataBasePort = parser.get_value<int>("DataBase.Port");


		FinderAddress = parser.get_value<std::string>("Finder.Address");
		FinderPort = parser.get_value<int>("Finder.Port");


		std::cout << "DataBaseHostName: " << DataBaseHostName << std::endl;
		std::cout << "DataBaseName: " << DataBaseName << std::endl;
		std::cout << "DataBaseUserName: " << DataBaseUserName << std::endl;
		std::cout << "DataBasePassword: " << DataBasePassword << std::endl;
		std::cout << "DataBasePort: " << DataBasePort << std::endl;
		std::cout << "FinderAddress: " << FinderAddress << std::endl;
		std::cout << "FinderPort: " << FinderPort << std::endl;


		auto const address = net::ip::make_address(FinderAddress);
		auto const port = static_cast<unsigned short>(FinderPort);
		auto const doc_root = std::make_shared<std::string>(".");
		auto const threads = std::max<int>(1, 1);


		try {
			DB.SetConnection(DataBaseHostName, DataBaseName, DataBaseUserName, DataBasePassword, DataBasePort);
		}
		catch (const std::exception& ex) {
			std::cout << "Try to create tables in databse\n";
			std::string except = ex.what();
			std::cout << "\n" << except;
		}

		// The io_context is required for all I/O
		net::io_context ioc{ threads };

		// Create and launch a listening port
		std::make_shared<listener>(
			ioc,
			tcp::endpoint{ address, port },
			doc_root)->run();

		// Run the I/O service on the requested number of threads
		std::vector<std::thread> v;
		v.reserve(threads - 1);
		for (auto i = threads - 1; i > 0; --i)
			v.emplace_back(
				[&ioc]
				{
					ioc.run();
				});
		ioc.run();

		return EXIT_SUCCESS;
	}
	catch (const std::exception& ex) {

		std::string except = ex.what();
		std::cout << "\n" << except;
	}

}