#include "g_database.h"
#include "sqlite3.h"
#include "time.h"

static sqlite3* db = 0;

const char* const databaseFileName = "enhanced.db";

const char* const sqlCreateDatabase =
"CREATE TABLE[accounts](                                                        "
"[account_id] INTEGER PRIMARY KEY AUTOINCREMENT,                                "
"[username] TEXT,                                                               "
"[public_name] TEXT,                                                            "
"[guid] TEXT );                                                                 "
"                                                                               "
"                                                                               "
"CREATE TABLE[sessions](                                                        "
"    [session_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            "
"    [session_start] DATETIME,                                                  "
"    [ip_text] TEXT );                                                          "
"                                                                               "
"                                                                               "
"CREATE TABLE[account_sessions](                                                "
"    [account_id] INTEGER REFERENCES[accounts]( [account_id] ),                 "
"    [session_id] INTEGER REFERENCES[sessions]( [session_id] ) );               "
"                                                                               "
"                                                                               "
"CREATE TABLE[hack_attempts](                                                   "
"    [session_id] INTEGER REFERENCES[sessions]( [session_id] ),                 "
"    [ip_text] TEXT,                                                            "
"    [description] TEXT );                                                      "
"                                                                               "
"                                                                               "
"CREATE TABLE[ip_blacklist](                                                    "
"    [ip_A] INTEGER( 255 ),                                                     "
"    [ip_B] INTEGER( 255 ),                                                     "
"    [ip_C] INTEGER( 255 ),                                                     "
"    [ip_D] INTEGER( 255 ),                                                     "
"    [mask_A] INTEGER( 255 ),                                                   "
"    [mask_B] INTEGER( 255 ),                                                   "
"    [mask_C] INTEGER( 255 ),                                                   "
"    [mask_D] INTEGER( 255 ),                                                   "
"    [notes] TEXT,                                                              "
"    [reason] TEXT,                                                             "
"    [banned_since] DATETIME,                                                   "
"    [banned_until] DATETIME );                                                 "
"                                                                               "
"                                                                               "
"CREATE TABLE[ip_whitelist](                                                    "
"    [ip_A] INTEGER( 255 ),                                                     "
"    [ip_B] INTEGER( 255 ),                                                     "
"    [ip_C] INTEGER( 255 ),                                                     "
"    [ip_D] INTEGER( 255 ),                                                     "
"    [mask_A] INTEGER( 255 ),                                                   "
"    [mask_B] INTEGER( 255 ),                                                   "
"    [mask_C] INTEGER( 255 ),                                                   "
"    [mask_D] INTEGER( 255 ) );                                                 "
"                                                                               "
"                                                                               "
"CREATE TABLE[levels](                                                          "
"    [level_id] INTEGER PRIMARY KEY AUTOINCREMENT,                              "
"    [started_at] DATETIME,                                                     "
"    [mapname] TEXT );                                                          "
"                                                                               "
"                                                                               "
"CREATE TABLE[session_events_enum](                                             "
"    [event_id] INTEGER PRIMARY KEY AUTOINCREMENT,                              "
"    [event_name] TEXT );                                                       "
"                                                                               "
"                                                                               "
"CREATE TABLE[session_events](                                                  "
"    [session_event_id] INTEGER PRIMARY KEY AUTOINCREMENT,                      "
"    [session_id] INTEGER REFERENCES[sessions]( [session_id] ),                 "
"    [event_time] DATETIME,                                                     "
"    [event_id] INTEGER REFERENCES[session_events_enum]( [event_id] ),          "
"    [event_context] TEXT );                                                    "
"                                                                               "
"                                                                               "
"CREATE TABLE[session_stats](                                                   "
"    [session_id] INTEGER REFERENCES[sessions]( [session_id] ),                 "
"    [accuracy_hits] INTEGER,                                                   "
"    [accuracy_shots] INTEGER );                                                "; 

const char* const sqlIsIpBlacklisted =
"SELECT reason                           "
"FROM ip_blacklist                       "
"WHERE( ip_A & mask_A ) = (? & mask_A)   "
"AND( ip_B & mask_B ) = (? & mask_B)     "
"AND( ip_C & mask_C ) = (? & mask_C)     "
"AND( ip_D & mask_D ) = (? & mask_D)     ";

const char* const sqlIsIpWhitelisted =
"SELECT COUNT(*)                         "
"FROM ip_whitelist                       "
"WHERE( ip_A & mask_A ) = (? & mask_A)   "
"AND( ip_B & mask_B ) = (? & mask_B)     "
"AND( ip_C & mask_C ) = (? & mask_C)     "
"AND( ip_D & mask_D ) = (? & mask_D)     ";

const char* const sqlAddToBlacklist =
"INSERT INTO ip_blacklist (ip_A, ip_B, ip_C, ip_D,        "
"mask_A, mask_B, mask_C, mask_D, notes)                   "
"VALUES (?,?,?,?,?,?,?,?,?)                               ";

const char* const sqlAddToWhitelist =
"INSERT INTO ip_whitelist (ip_A, ip_B, ip_C, ip_D, " 
"mask_A, mask_B, mask_C, mask_D)                   "
"VALUES (?,?,?,?,?,?,?,?)                          ";

const char* const sqlRemoveFromBlacklist =
"DELETE FROM ip_blacklist                                   "
"WHERE ip_A = ? AND ip_B = ? AND ip_C = ? AND ip_D = ?      "
"AND mask_A = ? AND mask_B = ? AND mask_C = ? AND mask_D = ?";

const char* const sqlremoveFromWhitelist =
"DELETE FROM ip_whitelist                                   "
"WHERE ip_A = ? AND ip_B = ? AND ip_C = ? AND ip_D = ?      "
"AND mask_A = ? AND mask_B = ? AND mask_C = ? AND mask_D = ?";

const char* const sqlLogLevel =
"INSERT INTO levels (started_at, mapname) "
"VALUES (?,?)                             ";

const char* const sqlAddSession =
"INSERT INTO sessions (session_start, ip_text)     "
"VALUES (?,?)                                      ";

const char* const sqlAddSessionEvent =
"INSERT INTO session_events (session_id, event_time, event_id, event_context)     "
"VALUES (?,?,?,?)                                                                 ";

//
//  G_DbLoad
// 
//  Loads the database from disk, including creating of not exists
//  or if it is corrupted
//
void G_DbLoad()
{    
    int rc = -1;    

    rc = sqlite3_initialize();
    rc = sqlite3_open_v2( databaseFileName, &db, SQLITE_OPEN_READWRITE, 0 );

    if ( rc != SQLITE_OK )
    {
        // create new database
        rc = sqlite3_open_v2( databaseFileName, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0 );

        sqlite3_exec( db, sqlCreateDatabase, 0, 0, 0 );          
    }
}


//
//  G_DbUnload
// 
//  Unloads the database from memory, includes flushing it 
//  to the file.
//
void G_DbUnload()
{
    int rc = -1;

    rc = sqlite3_close( db );
}

//
//  G_DbLogLevel
// 
//  Logs current level (map) to the database
//
void G_DbLogLevel()
{
    int rc = -1;
    sqlite3_stmt* statement;

    // load current map name
    char mapname[128];
    trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );

    // load current date time
    char dateTime[24];
    time_t rawtime;
    time( &rawtime );
    struct tm* timeinfo = localtime( &rawtime );
    strftime( dateTime, sizeof( dateTime ), "%Y-%m-%d %H:%M:%S.000", timeinfo );

    // prepare insert statement
    rc = sqlite3_prepare( db, sqlLogLevel, -1, &statement, 0 );
    sqlite3_bind_text( statement, 1, dateTime, -1, 0 );
    sqlite3_bind_text( statement, 2, mapname, -1, 0 );

    rc = sqlite3_step( statement );

    sqlite3_finalize( statement );

}

//
//  G_DbIsFiltered
// 
//  Checks if given ip is forbidden to join the game based
//  on blacklist/whitelist mechanisms.
//
qboolean G_DbIsFiltered( const char* ip, char* reasonBuffer, int reasonBufferSize )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int port = 0;
    int rc = -1;
    qboolean filtered = qfalse;

    // parse ip address
    if ( sscanf( ip, "%d.%d.%d.%d:%d", &ipA, &ipB, &ipC, &ipD, &port ) >= 4 )
    {
        if ( g_whitelist.integer )
        {
            filtered = G_DbIsFilteredByWhitelist( ipA, ipB, ipC, ipD, reasonBuffer, reasonBufferSize );
        }
        else
        {
            filtered = G_DbIsFilteredByBlacklist( ipA, ipB, ipC, ipD, reasonBuffer, reasonBufferSize );
        }
    }

    return filtered;
}

//
//  G_DbIsFilteredByWhitelist
// 
//  Helper method to check if given ip address is white listed
//  according to database.
//
qboolean G_DbIsFilteredByWhitelist( int ipA, int ipB, int ipC, int ipD, char* reasonBuffer, int reasonBufferSize )
{
    qboolean filtered = qfalse;

    // check if ip is on white list
    sqlite3_stmt* statement;

    // prepare whitelist check statement
    int rc = sqlite3_prepare( db, sqlIsIpWhitelisted, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipA );
    sqlite3_bind_int( statement, 2, ipB );
    sqlite3_bind_int( statement, 3, ipC );
    sqlite3_bind_int( statement, 4, ipD );

    rc = sqlite3_step( statement );
    int count = sqlite3_column_int( statement, 0 );

    if ( count == 0 )
    {
        Q_strncpyz( reasonBuffer, "IP address not on whitelist", reasonBufferSize );
        filtered = qtrue;
    }

    sqlite3_finalize( statement );

    return filtered;
}

//
//  G_DbIsFilteredByBlacklist
// 
//  Helper method to check if given ip address is black listed
//  according to database.
//
qboolean G_DbIsFilteredByBlacklist( int ipA, int ipB, int ipC, int ipD, char* reasonBuffer, int reasonBufferSize )
{
    qboolean filtered = qfalse;

    sqlite3_stmt* statement;

    // prepare blacklist check statement
    int rc = sqlite3_prepare( db, sqlIsIpBlacklisted, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipA );
    sqlite3_bind_int( statement, 2, ipB );
    sqlite3_bind_int( statement, 3, ipC );
    sqlite3_bind_int( statement, 4, ipD );

    rc = sqlite3_step( statement );

    // blacklisted => we forbid it
    if ( rc == SQLITE_ROW )
    {
        const char* reason = sqlite3_column_text( statement, 0 ); 
        const char* prefix = "Banned: ";
        int prefixSize = strlen( prefix );

        Q_strncpyz( reasonBuffer, prefix, reasonBufferSize );
        Q_strncpyz( &reasonBuffer[prefixSize], reason, reasonBufferSize - prefixSize );
        filtered = qtrue;
    }

    sqlite3_finalize( statement );

    return filtered;
}

//
//  G_DbAddToWhitelist
// 
//  Adds ip address to whitelist
//
void G_DbAddToWhitelist( const char* ip, const char* mask )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlAddToWhitelist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        rc = sqlite3_step( statement );

        sqlite3_finalize( statement );
    }  
}

//
//  G_DbAddToBlacklist
// 
//  Adds ip address to blacklist
//
void G_DbAddToBlacklist( const char* ip, const char* mask, const char* notes )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlAddToBlacklist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        sqlite3_bind_text( statement, 9, notes, -1, 0 );

        rc = sqlite3_step( statement );

        sqlite3_finalize( statement );
    }
}

//
//  G_DbRemoveFromBlacklist
// 
//  Removes ip address from blacklist
//
void G_DbRemoveFromBlacklist( const char* ip,
    const char* mask )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlRemoveFromBlacklist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        rc = sqlite3_step( statement );

        sqlite3_finalize( statement );

    }
}

//
//  G_DbRemoveFromWhitelist
// 
//  Removes ip address from whitelist
//
void G_DbRemoveFromWhitelist( const char* ip,
    const char* mask )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlremoveFromWhitelist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        rc = sqlite3_step( statement );

        sqlite3_finalize( statement );

    }
}

//
//  G_DbLogSession
// 
//  Logs players connection session
//
int G_DbLogSession( const char* ip )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddSession, -1, &statement, 0 );

    // load current date time
    char dateTime[24];
    time_t rawtime;
    time( &rawtime );
    struct tm* timeinfo = localtime( &rawtime );
    strftime( dateTime, sizeof( dateTime ), "%Y-%m-%d %H:%M:%S.000", timeinfo );

    sqlite3_bind_text( statement, 1, dateTime, -1, 0 );
    sqlite3_bind_text( statement, 2, ip, -1, 0 );

    rc = sqlite3_step( statement );

    int sessionId = sqlite3_last_insert_rowid( db );

    sqlite3_finalize( statement );
    
    return sessionId;
}

//
//  G_DbLogSessionEvent
// 
//  Logs players connection session event
//
int G_DbLogSessionEvent( int sessionId,
    int eventId,
    const char* eventContext )  
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddSessionEvent, -1, &statement, 0 );

    // load current date time
    char dateTime[24];
    time_t rawtime;
    time( &rawtime );
    struct tm* timeinfo = localtime( &rawtime );
    strftime( dateTime, sizeof( dateTime ), "%Y-%m-%d %H:%M:%S.000", timeinfo );

    sqlite3_bind_int( statement, 1, sessionId);
    sqlite3_bind_text( statement, 2, dateTime, -1, 0 );
    sqlite3_bind_int( statement, 3, eventId );
    sqlite3_bind_text( statement, 4, eventContext, -1, 0 );

    rc = sqlite3_step( statement );

    sqlite3_finalize( statement );

    return sessionId;
    
}