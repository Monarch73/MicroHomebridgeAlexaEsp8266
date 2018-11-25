#pragma once
class StrFunc
{
public:
	StrFunc();
	~StrFunc();
	static char* indexOf(char *, char*, size_t);
	static char* substrdup(char *, size_t );
private:

};

StrFunc::StrFunc()
{
}

StrFunc::~StrFunc()
{
}

char *StrFunc::substrdup(char *buf, size_t len)
{
	if (len > 0)
	{
		char *result = (char *)malloc(len + 1);
		if (!result)
		{
			Serial.println("memroy alloc failure");
			while (true);
		}
		result[len] = 0;
		memcpy(result, buf, len);
		return result;
	}
	return 0;
}

char* StrFunc::indexOf(char* buf, char*needle, size_t buflen)
{
	size_t needlelen = strlen(needle);
	if (needlelen == 0) return 0;
	size_t bufpospo = 0;
	size_t needlepos = 0;
	size_t scanpos;
	while (bufpospo < buflen)
	{
		scanpos = bufpospo;
		needlepos = 0;
		while (buf[scanpos] == needle[needlepos])
		{
			scanpos++;
			needlepos++;
			if (needlepos == needlelen)
			{
				return buf + bufpospo;
			}

			if (scanpos >= buflen)
			{
				return 0;
			}
		}
		
		bufpospo++;
	}

	return 0;
}