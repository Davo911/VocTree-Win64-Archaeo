//Copyright (C) 2016, Esteban Uriza <estebanuri@gmail.com>
//This program is free software: you can use, modify and/or
//redistribute it under the terms of the GNU General Public
//License as published by the Free Software Foundation, either
//version 3 of the License, or (at your option) any later
//version. You should have received a copy of this license along
//this program. If not, see <http://www.gnu.org/licenses/>.
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib") //Winsock Library
#include <windows.h>
#include <thread>
#include <tlhelp32.h>
#include <stdio.h>
#include "Server.h"
#include <process.h>
#include "FileHelper.h"
#include <string>
//#include <netinet/in.h>
//#include <signal.h>
//#include <sys/socket.h>
//#include <unistd.h>
//#include <netdb.h>
#include <iostream>



using namespace std;
using namespace cv;


bool startsWith(string str, string prefix) {
    return (_stricmp(prefix.c_str(), str.substr(0, prefix.size()).c_str()) == 0);
}
string dropPrefix(string str, string prefix) {
    return str.substr(prefix.size());
}
void log(string message) {

    stringstream ss;
    ss << "[" << _getpid() << "] " << message << endl;
    cout << ss.str();

}
void kill_by_pid(int pid)
{
	HANDLE handy;
	handy = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, pid);
	TerminateProcess(handy, 0);
}
DWORD getppid()
{
	HANDLE hSnapshot = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 pe32;
	DWORD ppid = 0, pid = GetCurrentProcessId();

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	__try {
		if (hSnapshot == INVALID_HANDLE_VALUE) __leave;

		ZeroMemory(&pe32, sizeof(pe32));
		pe32.dwSize = sizeof(pe32);
		if (!Process32First(hSnapshot, &pe32)) __leave;

		do {
			if (pe32.th32ProcessID == pid) {
				ppid = pe32.th32ParentProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));

	}
	__finally {
		if (hSnapshot != INVALID_HANDLE_VALUE) CloseHandle(hSnapshot);
	}
	return ppid;
}
void sendMessage(SOCKET sockfd, string message) {

    log(message);

    stringstream ss;
    ss << message << endl;
	int n = send(sockfd, ss.str().c_str(), ss.str().size(), 0);
    //auto n = write(sockfd, ss.str().c_str(), ss.str().size());
    if (n < 0) {
        cerr << "SENDING MSG: error writing to socket" << endl;
        exit(1);
    }

}

string
sendCommand(string host, int port, string command) {
	SOCKET sockfd;
	WSADATA wsa;
    struct sockaddr_in serv_addr;
    struct hostent *server;

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (iResult != 0) {
		wprintf(L"WSAStartup failed: %d\n", iResult);
		exit(1);
	}
	//Create a socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		cerr << "Could not create socket : %d" << WSAGetLastError();
	}

	cout << "Socket created" << endl;

    //Prepare the sockaddr_in structure
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    server = gethostbyname(host.c_str());
	memcpy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    // Now connect to the server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))  == SOCKET_ERROR) {
		int err = WSAGetLastError();
		char msgbuf[256];
		msgbuf[0] = '\0';
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
			NULL,                // lpsource
			err,                 // message id
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),    // languageid
			msgbuf,              // output buffer
			sizeof(msgbuf),     // size of msgbuf, bytes
			NULL);               // va_list of arguments

		cerr << "ERROR connecting:    " << msgbuf << endl;

        throw (-1);
    }
	
    //int n = write(sockfd, command.c_str(), command.length());
	int n = send(sockfd, command.c_str(), command.length(), 0);
    if (n < 0) {
        cerr << "ERROR writing to socket" << endl;
        throw (-1);
    }

    // Now read server response
    char buffer[256];
    //bzero(buffer, 256);
	memset(buffer, 0 ,256);
    string ret = "";
    //while ((n = read(sockfd, buffer, 255)) > 0) {
	while ((n = recv(sockfd,buffer,255,0)) > 0){
        string s(buffer, n);
        ret += s;
    }

    if (n < 0) {
        cerr << "ERROR reading from socket" << endl;
        throw (-1);
    }

    //string response(buffer);
    //return response;
    return ret;

}


string
readCommand(int newsockfd) {

    int n;
    char buffer[256];

    //bzero(buffer, 256);
	memset(buffer, 0, 256);
    //n = read(newsockfd, buffer, 256);
	n = recv(newsockfd, buffer, 256, 0);
    if (n < 0) {
        cerr << "error reading from socket." << endl;
        exit(1);
    }

    string command(buffer);
    command.erase(command.find_last_not_of(" \n\r\t") + 1);

    return command;

}


void handleQuery(string query, SOCKET sockfd, Ptr<Database> &db) {

    //msg()
    string msg;
    msg = "executing query: " + query;
    sendMessage(sockfd, msg);


    string fileQuery;
    if (startsWith(query, "...")) {
        query = dropPrefix(query, "...");
        fileQuery += db->getPath();
    }
    fileQuery += query;

    //int limit = 6; //result.size() > 10 ? 10 : result.size();
    int limit = 16; //result.size() > 10 ? 10 : result.size();

    vector<Matching> result;
    db->query(fileQuery, result, limit);

    vector<Database::ExportInfo> exports = db->exportResults(result);
    assert(result.size() == exports.size());

    cout << "query done." << endl;
    cout << "result size:" << result.size() << endl;

    for (unsigned int i = 0; i < result.size(); i++) {

        Matching m = result.at(i);
        Database::ExportInfo info = exports.at(i);

        //cout << "matching:" << i << ", " << m.id << ", " << m.score << endl;

        float score = m.score;
        //DBElem info = db->getFileInfo( m.id );
        //cout << score << " " << info.name << endl;

        stringstream ss;
        ss << score << "," << m.id << ", " << info.fileName << endl;

        //int n = write(sockfd, ss.str().c_str(), ss.str().size());
		int n = send(sockfd, ss.str().c_str(), ss.str().size(), 0);
        if (n < 0) {
            cerr << "writing response: error writing to socket" << endl;
            exit(1);
        }


    }

}


void handleCommand(string command, SOCKET sockfd, Ptr<Database> &db) {

    //bool breaks = false;

    if (_stricmp(command.c_str(), "exit") == 0 ||
        _stricmp(command.c_str(), "quit") == 0) {
        sendMessage(sockfd, "good bye.");
        //breaks = true;
    } else if (_stricmp(command.c_str(), "term") == 0) {
        sendMessage(sockfd, "terminating server.");
        //kill(getppid(), SIGTERM);
		kill_by_pid(getppid());
        //breaks = true;
    } else if (startsWith(command, "query ")) {
        string query = dropPrefix(command, "query ");
        handleQuery(query, sockfd, db);
    } else {
        string msg = "unknown command '" + command + "'.";
        sendMessage(sockfd, msg);
    }


}


void processClient(SOCKET sockfd, Ptr<Database> &db) {

    string msg = "started";
    log("process start");

    string command = readCommand(sockfd);

    if (_stricmp(command.c_str(), "hello") == 0) {
        sendMessage(sockfd, "hello :)");
    } else {
        handleCommand(command, sockfd, db);
    }

    //close(sockfd);
	int iResult = closesocket(sockfd);
	if (iResult == SOCKET_ERROR) {
		wprintf(L"close failed with error: %d\n", WSAGetLastError());
		WSACleanup();
		exit(1);
	}

    log("process end");


}

Configuration
readConfig(string dbPath) {

    string configFile;
    configFile = dbPath + "/config.txt";

    if (!FileHelper::exists(configFile)) {
        cerr << "could not find database configuration file " << configFile << "." << endl;
        exit(-1);
    }

    return Configuration(configFile);

}

void listenForClients(int port, Ptr<Database> db) {
    struct sockaddr_in serv_addr;
	WSADATA wsa;

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (iResult != 0) {
		wprintf(L"WSAStartup failed: %d\n", iResult);
		exit(1);
	}

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        cerr << "error opening socket." << endl;
        exit(1);
    }

    //Initialize socket structure
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);


    // re-use socket when has been killed recently
    int val = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int));

    // sets socket timeout.
    // if it doesn't receive queries for a while, it auto-terminates
    struct timeval timeout;
    timeout.tv_sec = 5 * 60;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0) {
        cerr << "set socket option failed" << endl;
        exit(1);
    }


    //Now bind the host address using bind() call
    if (::bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        cerr << "cannot bind socket." << endl;
        exit(1);
    }

    struct sockaddr_in cli_addr;
    int clilen;

    clilen = sizeof(cli_addr); 
    cout << "database listening on port: " << port << endl;

    listen(sockfd, 5);

	bool term = false;
	while (!term) {
		SOCKET newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd == INVALID_SOCKET) {
			cout << "terminating server due to inactivity of (" << timeout.tv_sec << ") secs." << endl;
			term = true;
			continue;


			std::thread child([sockfd, &cli_addr, &clilen, timeout, newsockfd] {

				int iResult = closesocket(newsockfd);
				if (iResult == SOCKET_ERROR) {
					wprintf(L"close failed with error: %d\n", WSAGetLastError());
					WSACleanup();
					exit(1);
				}
			});

			//This is the client process
			//close(sockfd);
			int iResult = closesocket(sockfd);
			if (iResult == SOCKET_ERROR) {
				wprintf(L"close failed with error: %d\n", WSAGetLastError());
				WSACleanup();
				exit(1);
			}

			processClient(newsockfd, db);

			cout << "process query" << endl;
			exit(0);
		}
	}



        /*/Create child process
        int pid = fork();
        if (pid < 0) {
            cerr << "error creating new process (fork)." << endl;
            exit(1);
        }

        if (pid == 0) {
            //This is the client process
            //close(sockfd);
			int iResult = closesocket(sockfd);
			if (iResult == SOCKET_ERROR) {
				wprintf(L"close failed with error: %d\n", WSAGetLastError());
				WSACleanup();
				exit(1);
			}

            processClient(newsockfd, db);

            cout << "process query" << endl;
            exit(0);

        } else {

            //close(newsockfd);
			int iResult = closesocket(newsockfd);
			if (iResult == SOCKET_ERROR) {
				wprintf(L"close failed with error: %d\n", WSAGetLastError());
				WSACleanup();
				exit(1);
			}

        }*/

}


int getPort(string dbPath) {

    // Reads database configuration
    Configuration cfg = readConfig(dbPath);

    if (!cfg.has("port")) {
        cerr << "no port configured" << endl;
        exit(1);
    }

    int port = atoi(cfg.get("port").c_str());

    return port;

}


bool isStarted(int port) {

    string host = "localhost";
    string command = "hello";

    try {
        string ret = sendCommand(host, port, command);
		cout << "Testcommand: " << command << "sent" << endl;
        return true;
    }
    catch (const std::exception& e) {
		cout << e.what();
        return false;
    }

}

string startingLockName(string dbPath) {
    return dbPath + "/starting.lock";
}

bool isStarting(string dbPath) {
    string lock = startingLockName(dbPath);
    return (FileHelper::exists(lock));
}

void setStartingLock(string dbPath) {
    string lock = startingLockName(dbPath);
    ofstream file(lock.c_str());
}

void delStartingLock(string dbPath) {
    string lock = startingLockName(dbPath);
    FileHelper::deleteFile(lock);
}

void startDatabase(string dbPath) {
	cout << "Attempting to start the Database..." << endl;

    int port = getPort(dbPath);
	cout << "Got Port :" << port << endl;

    if (isStarted(port)) {
        cout << "database is STARTED" << endl;
        return;
    }
	cout << "isStarted -- DONE" << endl;

    if (isStarting(dbPath)) {
        cout << "database is STARTING" << endl;
        return;
    }

	cout << "isStarting -- DONE" << endl;

    // Ok, the server is stopped.

    setStartingLock(dbPath);

    cout << "starting server..." << endl;

    Ptr<Database> db;
    cout << "loading database " << dbPath << "..." << endl << flush;
    db = Database::load(dbPath);
    cout << "load done." << endl << flush;

    delStartingLock(dbPath);


    listenForClients(port, db);


}


string getState(string dbPath) {

    int port = getPort(dbPath);

    if (isStarted(port)) {
        return "STARTED";
    }

    if (isStarting(dbPath)) {
        return "STARTING";
    }

    return "STOPPED";

}


void runQuery(string dbPath, string query) {

    int port = getPort(dbPath);

    string host = "localhost";
    string command = "query " + query;
    try {

        string ret = sendCommand(host, port, command);
        cout << ret << endl;

    }
    catch (int ex) {

        // can't run command.
        if (isStarting(dbPath)) {
            cout << "database is STARTING" << endl;
        } else {
            cout << "database is STOPPED" << endl;
        }


    }

}


void stopDatabase(string dbPath) {

    int port = getPort(dbPath);

    if (!isStarted(port)) {

        cout << "database is not STARTED" << endl;
        return;

    }

    string host = "localhost";
    string command = "term";
    try {
        string ret = sendCommand(host, port, command);
        cout << ret << endl;
    }
    catch (int ex) {
        cout << "can't stop database" << endl;
    }

}