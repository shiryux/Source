#include "CDataBase.h"
#include "../sphere/asyncdb.h"
#include <mysql_version.h>

extern CDataBaseAsyncHelper g_asyncHdb;

CDataBase::CDataBase()
{
	m_connected = false;
	m_socket = NULL;
}

CDataBase::~CDataBase()
{
	if ( m_connected )
		Close();
}

void CDataBase::Connect()
{
	ADDTOCALLSTACK("CDataBase::Connect");
	SimpleThreadLock lock(m_connectionMutex);
	if ( m_connected )
		return;

	if ( mysql_get_client_version() < LIBMYSQL_VERSION_ID )
	{
#ifdef _WIN32
		g_Log.EventWarn("Your MySQL client library %s is outdated. For better compatibility, update your 'libmysql.dll' file to version %s\n", mysql_get_client_info(), LIBMYSQL_VERSION);
#else
		g_Log.EventWarn("Your MySQL client library %s is outdated. For better compatibility, update your 'libmysqlclient' package to version %s\n", mysql_get_client_info(), LIBMYSQL_VERSION);
#endif
	}

	m_socket = mysql_init(NULL);
	if ( !m_socket )
	{
		g_Log.EventError("Insufficient memory to initialize MySQL client socket\n");
		return;
	}

	const char *user = g_Cfg.m_sMySqlUser;
	const char *password = g_Cfg.m_sMySqlPass;
	const char *db = g_Cfg.m_sMySqlDB;
	const char *host = g_Cfg.m_sMySqlHost;
	unsigned int port = MYSQL_PORT;

	// If user define server port using hostname:port format, split values into different variables
	const char *pszArgs = strchr(host, ':');
	if ( pszArgs != NULL )
	{
		char *pszTemp = Str_GetTemp();
		strcpy(pszTemp, host);
		*(strchr(pszTemp, ':')) = 0;
		port = ATOI(pszArgs + 1);
		host = pszTemp;
	}

	if ( mysql_real_connect(m_socket, host, user, password, db, port, NULL, CLIENT_MULTI_STATEMENTS) )
	{
		m_connected = true;
		if ( mysql_get_server_version(m_socket) < MYSQL_VERSION_ID )
			g_Log.EventWarn("Your MySQL server %s is outdated. For better compatibility, update your MySQL server to version %s\n", mysql_get_server_info(m_socket), MYSQL_SERVER_VERSION);
	}
	else
	{
		g_Log.EventError("MySQL error #%u: %s\n", mysql_errno(m_socket), mysql_error(m_socket));
		mysql_close(m_socket);
		m_socket = NULL;
	}
}

void CDataBase::Close()
{
	ADDTOCALLSTACK("CDataBase::Close");
	SimpleThreadLock lock(m_connectionMutex);
	mysql_close(m_socket);
	m_connected = false;
	m_socket = NULL;
}

bool CDataBase::Query(const char *query, CVarDefMap &mapQueryResult)
{
	ADDTOCALLSTACK("CDataBase::Query");
	mapQueryResult.Empty();
	mapQueryResult.SetNumNew("NUMROWS", 0);

	if ( !m_connected )
		return false;

	// Connection can only handle one query at a time, so lock the thread until the query finishes
	SimpleThreadLock lock(m_connectionMutex);

	int resultCode = mysql_query(m_socket, query);
	if ( resultCode == 0 )
	{
		MYSQL_RES *result = mysql_store_result(m_socket);
		if ( !result )
			return false;

		unsigned int num_fields = mysql_num_fields(result);
		char key[64];
		if ( num_fields > COUNTOF(key) )
		{
			// This check is not really needed, MySQL client should be able to handle the same columns amount of MySQL server (4096).
			// But since this value is too big and create an 4096 char array at -every- query call is not performance-friendly, maybe
			// it's better just use an smaller array to prioritize performance.
			g_Log.EventError("MySQL query returned too many columns [Max: %" FMTSIZE_T " / Cmd: \"%s\"]\n", COUNTOF(key), query);
			mysql_free_result(result);
			return false;
		}

		MYSQL_FIELD *fields = mysql_fetch_fields(result);
		mapQueryResult.SetNum("NUMROWS", mysql_num_rows(result));
		mapQueryResult.SetNum("NUMCOLS", num_fields);

		int rownum = 0;
		char **row = NULL;
		char *pszKey = Str_GetTemp();
		char *pszVal = NULL;
		while ( (row = mysql_fetch_row(result)) != NULL )
		{
			for ( unsigned int i = 0; i < num_fields; i++ )
			{
				pszVal = row[i];
				if ( !rownum )
				{
					mapQueryResult.SetStr(ITOA(i, key, 10), true, pszVal);
					mapQueryResult.SetStr(fields[i].name, true, pszVal);
				}

				sprintf(pszKey, "%d.%u", rownum, i);
				mapQueryResult.SetStr(pszKey, true, pszVal);
				sprintf(pszKey, "%d.%s", rownum, fields[i].name);
				mapQueryResult.SetStr(pszKey, true, pszVal);
			}
			rownum++;
		}

		mysql_free_result(result);
		return true;
	}
	else
	{
		g_Log.EventError("MySQL error #%u: %s [Cmd: \"%s\"]\n", mysql_errno(m_socket), mysql_error(m_socket), query);
		if ( (resultCode == CR_SERVER_GONE_ERROR) || (resultCode == CR_SERVER_LOST) )
			Close();

		return false;
	}
}

bool __cdecl CDataBase::Queryf(CVarDefMap &mapQueryResult, char *fmt, ...)
{
	ADDTOCALLSTACK("CDataBase::Queryf");
	TemporaryString buf;
	va_list	marker;

	va_start(marker, fmt);
	_vsnprintf(buf, buf.realLength(), fmt, marker);
	va_end(marker);

	return Query(buf, mapQueryResult);
}

bool CDataBase::Exec(const char *query)
{
	ADDTOCALLSTACK("CDataBase::Exec");
	if ( !m_connected )
		return false;

	// Connection can only handle one query at a time, so lock the thread until the query finishes
	SimpleThreadLock lock(m_connectionMutex);

	int resultCode = mysql_query(m_socket, query);
	if ( resultCode == 0 )
	{
		// Result must be retrieved from server even when no data is expected, otherwise the server will think the client has lost connection
		MYSQL_RES *result = mysql_store_result(m_socket);
		if ( !result )
			return false;

		mysql_free_result(result);
		return true;
	}
	else
	{
		g_Log.EventError("MySQL error #%u: %s [Cmd: \"%s\"]\n", mysql_errno(m_socket), mysql_error(m_socket), query);
		if ( (resultCode == CR_SERVER_GONE_ERROR) || (resultCode == CR_SERVER_LOST) )
			Close();

		return false;
	}
}

bool __cdecl CDataBase::Execf(char *fmt, ...)
{
	ADDTOCALLSTACK("CDataBase::Execf");
	TemporaryString buf;
	va_list	marker;

	va_start(marker, fmt);
	_vsnprintf(buf, buf.realLength(), fmt, marker);
	va_end(marker);

	return Exec(buf);
}

bool CDataBase::AsyncQueue(bool isQuery, LPCTSTR function, LPCTSTR query)
{
	ADDTOCALLSTACK("CDataBase::AsyncQueue");
	if ( !g_Cfg.m_Functions.ContainsKey(function) )
	{
		g_Log.EventError("Invalid %s callback function '%s'\n", isQuery ? "AQUERY" : "AEXECUTE", function);
		return false;
	}

	if ( !g_asyncHdb.isActive() )
		g_asyncHdb.start();

	g_asyncHdb.addQuery(isQuery, function, query);
	return true;
}

void CDataBase::AsyncQueueCallback(CGString &function, CScriptTriggerArgs *result)
{
	ADDTOCALLSTACK("CDataBase::AsyncQueueCallback");
	SimpleThreadLock lock(m_resultMutex);

	m_QueryArgs.push(FunctionArgsPair_t(function, result));
}

bool CDataBase::OnTick()
{
	ADDTOCALLSTACK("CDataBase::OnTick");
	static int tickcnt = 0;
	EXC_TRY("Tick");

	if ( !g_Cfg.m_bMySql )
		return true;

	// Periodically check if connection still active
	if ( ++tickcnt >= 1000 )
	{
		tickcnt = 0;
		if ( m_connected )
		{
			SimpleThreadLock lock(m_connectionMutex);
			if ( mysql_ping(m_socket) != 0 )
			{
				g_Log.EventError("MySQL server connection has been lost. Trying to reconnect...\n");
				Close();
				Connect();
			}
		}
	}

	if ( !m_QueryArgs.empty() && !(tickcnt % TICK_PER_SEC) )
	{
		SimpleThreadLock lock(m_resultMutex);
		FunctionArgsPair_t currentPair = m_QueryArgs.front();
		m_QueryArgs.pop();

		g_Serv.r_Call(currentPair.first, &g_Serv, currentPair.second);
		ASSERT(currentPair.second != NULL);
		delete currentPair.second;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_DEBUG_END;
	return false;
}

enum DBO_TYPE
{
	DBO_AEXECUTE,
	DBO_AQUERY,
	DBO_CONNECTED,
	DBO_ESCAPEDATA,
	DBO_ROW,
	DBO_QTY
};

LPCTSTR const CDataBase::sm_szLoadKeys[DBO_QTY + 1] =
{
	"AEXECUTE",
	"AQUERY",
	"CONNECTED",
	"ESCAPEDATA",
	"ROW",
	NULL
};

enum DBOV_TYPE
{
	DBOV_CLOSE,
	DBOV_CONNECT,
	DBOV_EXECUTE,
	DBOV_QUERY,
	DBOV_QTY
};

LPCTSTR const CDataBase::sm_szVerbKeys[DBOV_QTY + 1] =
{
	"CLOSE",
	"CONNECT",
	"EXECUTE",
	"QUERY",
	NULL
};

bool CDataBase::r_GetRef(LPCTSTR &pszKey, CScriptObj *&pRef)
{
	ADDTOCALLSTACK("CDataBase::r_GetRef");
	UNREFERENCED_PARAMETER(pszKey);
	UNREFERENCED_PARAMETER(pRef);
	return false;
}

bool CDataBase::r_LoadVal(CScript &s)
{
	ADDTOCALLSTACK("CDataBase::r_LoadVal");
	UNREFERENCED_PARAMETER(s);
	return false;
}

bool CDataBase::r_WriteVal(LPCTSTR pszKey, CGString &sVal, CTextConsole *pSrc)
{
	ADDTOCALLSTACK("CDataBase::r_WriteVal");
	EXC_TRY("WriteVal");

	if ( !g_Cfg.m_bMySql )
	{
		sVal.FormatVal(0);
		return true;
	}

	int index = FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);
	switch ( index )
	{
		case DBO_AEXECUTE:
		case DBO_AQUERY:
		{
			pszKey += strlen(sm_szLoadKeys[index]);
			GETNONWHITESPACE(pszKey);

			TCHAR *ppArgs[2];
			if ( (pszKey[0] != '\0') && (Str_ParseCmds(const_cast<TCHAR *>(pszKey), ppArgs, COUNTOF(ppArgs)) == 2) )
				sVal.FormatVal(AsyncQueue((index == DBO_AQUERY), ppArgs[0], ppArgs[1]));
			else
			{
				g_Log.EventError("Invalid %s arguments\n", CDataBase::sm_szLoadKeys[index]);
				sVal.FormatVal(0);
			}
			break;
		}
		case DBO_CONNECTED:
		{
			sVal.FormatVal(m_connected);
			break;
		}
		case DBO_ESCAPEDATA:
		{
			pszKey += strlen(sm_szLoadKeys[index]);
			GETNONWHITESPACE(pszKey);
			sVal = "";

			if ( pszKey[0] != '\0' )
			{
				TCHAR *escapedString = Str_GetTemp();
				SimpleThreadLock lock(m_connectionMutex);
				if ( m_connected && mysql_real_escape_string(m_socket, escapedString, pszKey, static_cast<unsigned long>(strlen(pszKey))) )
					sVal = escapedString;
			}
			break;
		}
		case DBO_ROW:
		{
			pszKey += strlen(sm_szLoadKeys[index]);
			SKIP_SEPARATORS(pszKey);
			sVal = m_QueryResult.GetKeyStr(pszKey);
			break;
		}
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	g_Log.EventDebug("command '%s' [%p]\n", pszKey, static_cast<void *>(pSrc));
	EXC_DEBUG_END;
	return false;
}

bool CDataBase::r_Verb(CScript &s, CTextConsole *pSrc)
{
	ADDTOCALLSTACK("CDataBase::r_Verb");
	EXC_TRY("Verb");

	if ( !g_Cfg.m_bMySql )
		return true;

	int index = FindTableSorted(s.GetKey(), sm_szVerbKeys, COUNTOF(sm_szVerbKeys) - 1);
	switch ( index )
	{
		case DBOV_CLOSE:
			if ( m_connected )
				Close();
			break;

		case DBOV_CONNECT:
			if ( !m_connected )
				Connect();
			break;

		case DBOV_EXECUTE:
			Exec(s.GetArgRaw());
			break;

		case DBOV_QUERY:
			Query(s.GetArgRaw(), m_QueryResult);
			break;

		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	g_Log.EventDebug("command '%s' args '%s' [%p]\n", s.GetKey(), s.GetArgRaw(), static_cast<void *>(pSrc));
	EXC_DEBUG_END;
	return false;
}
