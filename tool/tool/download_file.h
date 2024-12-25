#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include <barkeep.h>

namespace vcwin
{

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace asio = boost::asio;
    namespace ssl = asio::ssl;
    using tcp = asio::ip::tcp;

    // Function to parse the URL (extract host and path from the full HTTPS URL)
    inline void parse_https_url(const std::string &url, std::string &host, std::string &target, std::string &port)
    {
        // Parse the URL and extract host, target path and port
        size_t https_pos = url.find("https://");
        if (https_pos == std::string::npos)
        {
            throw std::invalid_argument("URL must start with https://");
        }

        std::string url_without_https = url.substr(8); // Remove "https://"

        // Extract host and path
        size_t slash_pos = url_without_https.find('/');
        if (slash_pos == std::string::npos)
        {
            host = url_without_https;
            target = "/";
        }
        else
        {
            host = url_without_https.substr(0, slash_pos);
            target = url_without_https.substr(slash_pos);
        }

        // Set the default port for HTTPS if not specified
        port = "443"; // Default HTTPS port
    }

    inline void download_file_with_progress(const std::string &url, const std::string &file_name)
    {
        try
        {

            std::string host, target, port;
            parse_https_url(url, host, target, port);

            asio::io_context ioc;
            ssl::context ctx(ssl::context::tls_client);
            ssl::stream<asio::ip::tcp::socket> stream(ioc, ctx);

            tcp::resolver resolver(ioc);
            auto const results = resolver.resolve(host, port);
            asio::connect(stream.next_layer(), results.begin(), results.end());

            // Perform SSL handshake
            stream.handshake(ssl::stream_base::client);

            // Create the HTTP GET request
            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            // Send the HTTP request
            http::write(stream, req);

            // Receive the HTTP response
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;

            http::response_parser<http::dynamic_body> parser;
            parser.body_limit(std::numeric_limits<std::uint64_t>::max());

            http::read_some(stream, buffer, parser);
            res = parser.release();

            // Extract the Content-Length header for progress tracking
            auto content_length = res.find(http::field::content_length);
            while (content_length == res.end())
            {
                http::read_some(stream, buffer, parser);
                res = parser.release();

                content_length = res.find(http::field::content_length);
            }

            size_t total_size = std::stoll(content_length->value());
            size_t downloaded = 0;

            // Open the file to save the download
            std::ofstream out_file(file_name, std::ios::binary);
            if (!out_file.is_open())
            {
                std::cerr << "Failed to open file for writing." << std::endl;
                return;
            }

            // Write the response body (download the file)
            // We must iterate through the buffers in the dynamic_body
            for (auto const &buffer_piece : res.body().data())
            {
                out_file.write(boost::asio::buffer_cast<const char *>(buffer_piece),
                               boost::asio::buffer_size(buffer_piece));
                downloaded += boost::asio::buffer_size(buffer_piece);
            }

            namespace bk = barkeep;

            size_t progress = downloaded / 1024 / 1024;
            auto bar = barkeep::ProgressBar<size_t>(&progress, {
                                                               .total = total_size / 1024 / 1024,
                                                               .message = "Downloading",
                                                               .speed = 1.0,
                                                               .speed_unit = "MB/s",
                                                               .style = bk::ProgressBarStyle::Rich,
                                                               
                                                           });

            // Progress tracking
            while (downloaded < total_size)
            {
                // Continue reading until the download is complete
                http::read_some(stream, buffer, parser);
                res = parser.release();

                for (auto const &buffer_piece : res.body().data())
                {
                    out_file.write(boost::asio::buffer_cast<const char *>(buffer_piece),
                                   boost::asio::buffer_size(buffer_piece));
                    downloaded += boost::asio::buffer_size(buffer_piece);
                    progress = downloaded / 1024 / 1024;
                }
            }

            bar->done();
            stream.shutdown();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

} // namespace vcwin