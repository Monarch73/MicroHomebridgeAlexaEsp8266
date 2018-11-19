#pragma once

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include <ESP8266WebServer.h>

class WcFnRequestHandler : public RequestHandler
{

public:
	typedef std::function<void(WcFnRequestHandler *handler, String requestUri, HTTPMethod method)> HandlerFunction;
	WcFnRequestHandler(HandlerFunction& fn, const String& uri, HTTPMethod& method, char wildcard = '*') : _fn(fn), _uri(uri), _method(method), _wildcard(wildcard)
	{

	}

	bool canHandle(HTTPMethod requestMethod, String requestUri) override
	{
		if (_method != HTTP_ANY && _method != requestMethod) {
			return false;
		}

		String uri = removeSlashes(_uri);
		requestUri = removeSlashes(requestUri);
		String wildcardStr;
		wildcardStr += _wildcard;
		while (1) {
			String uPath = getPathSegment(uri);
			String ruPath = getPathSegment(requestUri);
			if (uPath != ruPath && uPath != wildcardStr) {
				return false;
			}
			uri = removeSlashes(removePathSegment(uri));
			requestUri = removeSlashes(removePathSegment(requestUri));
			if (!uri.length() && !requestUri.length()) {
				return true;
			}
			if (!uri.length() || !requestUri.length()) {
				return false;
			}
		}

		return true;
	}

	bool canUpload(String requestUri) override
	{
		return false;
	}
	
	bool handle(ESP8266WebServer& server, HTTPMethod requestMethod, String requestUri) override
	{
		currentReqUri = requestUri;
		_fn(this, requestUri, requestMethod);
		currentReqUri = "";
		return true;
	}
	
	void upload(ESP8266WebServer& server, String requestUri, HTTPUpload& upload) override
	{
	}

	String getWildCard(int wcIndex)
	{
		return this->getWildCard(currentReqUri, _uri, wcIndex);

//		return getWildCard(currentReqUri, _uri, wcIndex);
	}

private:
	String getPathSegment(String uri)
	{
		// assume slashes removed
		int slash = uri.indexOf("/");
		if (slash == -1) {
			return uri;
		}
		return uri.substring(0, slash);
	}

	String removeSlashes(String uri)
	{
		if (uri[0] == '/') {
			uri = uri.substring(1);
		}
		
		if (uri.length() && uri[uri.length() - 1] == '/') {
			uri = uri.substring(0, uri.length() - 1);
		}
	
		return uri;
	}
	String removePathSegment(String uri)
	{
		// assume slashes removed
		int slash = uri.indexOf("/");
		if (slash == -1) {
			return "";
		}
		
		return uri.substring(slash);
	}

	String getWildCard(String requestUri, String wcUri, int n, char wildcard = '*')
	{
		wcUri = removeSlashes(wcUri);
		requestUri = removeSlashes(requestUri);
		String wildcardStr;
		wildcardStr += wildcard;
		int i = 0;
		while (1) {
			String uPath = getPathSegment(wcUri);
			String ruPath = getPathSegment(requestUri);
			if (uPath == wildcardStr) {
				if (i == n) {
					return ruPath;
				}
				i++;
			}
			wcUri = removeSlashes(removePathSegment(wcUri));
			requestUri = removeSlashes(removePathSegment(requestUri));
			if (!wcUri.length() && !requestUri.length()) {
				return "";
			}
			if (!wcUri.length() || !requestUri.length()) {
				return "";
			}
		}
		return "";
	}

protected:
	String currentReqUri;
	HandlerFunction _fn;
	String _uri;
	HTTPMethod _method;
	char _wildcard;
};
