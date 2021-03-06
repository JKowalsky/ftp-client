/** 
 * Session.cpp
 * @Author kowalsky
 * A session that a user accesses to issue commands to an FtP server.
 * The Receiver in the Command Pattern.
 * Created on November 30, 2014, 2:46 PM
 */

#include "Session.h"

using namespace std;
/**
 * constructor
 * initialize the session.
 */
Session::Session( int port, char ipName[] ) {
  this->username = "";
  this->authenticated = false;
  this->serverIp = ipName;
  this->ctrl = new Socket(port); // create a socket listening to that port

  // tell it the ip address to listen to.
  this->ctrlsd = this->ctrl->getClientSocket(ipName);

  if (this->ctrlsd == -1) {  // connection was unsuccessful
    cerr << "Error: Not connected to server." << endl;
  }
}

/**
 * destructor
 */
Session::~Session() {
  close(this->ctrlsd);
  this->ctrl = NULL;
}

/**
 * setUsername
 * Set the username for the session to a given string.
 * Whenever username is changed, isAuthenticated becomes false.
 * @param username new username for the session.
 * @return whether the username was accepted.
 */
bool Session::setUsername(string username) {
  // set our username to this username
  this->username = username;
  this->authenticated = false;
  bool success = true;

  sendCmdToServer(username);
  // read server's reply to the message
  string reply = getServerReply();
  // check if message is not 331
  if (!serverReplyEqualsCode(reply, "331")) {
      cerr << "Error in login." << endl;
      success = false;
  }
  
  return success;
}


/**
 * authenticate
 * using a password, authenticate the username associated with it
 * @param password password entered
 * @return whether or not the authentication was successful.
 */
bool Session::authenticate(string password) {
  // do authentication
  // let server know the password.
  authenticated = true; // by default

  sendCmdToServer(password);

  // read server's reply to the message
  string reply = getServerReply();
  // check if reply is 530, an error
  if (serverReplyEqualsCode(reply, "530")) {
    cerr << "Error in login." << endl;
    authenticated = false; // error in logging in
  }

  
  // once there is no more input, return
  return authenticated; // so caller knows if password was accepted
}

void Session::setPort(int p) {
  this->dataport = p;
}

/**
 * getUsername
 * Get the username for the session.
 * @return username associated with the session.
 */
string Session::getUsername() {
  return username;
}

/**
 * isAuthenticated
 * Returns whether or not a user is authenticated.
 * @return isAuthenticated or not
 */
bool Session::isAuthenticated() {
  return authenticated;
}

/**
 * sendCmdToServer
 * sends a given string cmd to the server.
 */
void Session::sendCmdToServer(string cmd) {
 
  // add CRLF to end of string
  string command = cmd + "\r\n";

  pid_t pid = fork();
  // let server know about the username.
  if (pid == 0) { // child process to check for spoof
    strcpy(message, command.c_str());
    write(ctrlsd, command.c_str(), command.length());
    close(ctrlsd); // close only in child
    exit(0);
  } else if (pid > 0 ) { // parent
    int returnStatus;    
    waitpid(pid, &returnStatus, 0);
  } else { // error in fork
    cerr << "Fork error in Session!" << endl;
  }
}


/**
 * getServerReply Prints the current server reply to console and
 * returns a string of the server message.  @return a string version
 * of the reply
 */
string Session::getServerReply() {
  string reply;

  // poll to see if there is a server reply so we can process multiple messages.
  while (ctrl->pollRecvFrom() > 0) {
    reply = doRead(ctrlsd);
  }
  return reply;
}


/**
 * getServerReply Prints the current server reply to console and
 * returns a string of the server message. Needed separate method
 * since closing reply is only one message long always.
 */
string Session::getServerClosingReply() {
  string reply;
  reply = doRead(ctrlsd);
  return reply;
}

/**
 * serverReplyEqualsCode
 * Compares code given by server to code entered by user.
 * @return true if a match, false if not.
 */
bool Session::serverReplyEqualsCode(string reply, string code) {
  
  // compare the 1st three characters of reply to the given code.
  if (reply.compare(0,3,code) == 0) {
    return true;
  } else {
    return false;
  }
}

void Session::setupDataSocket() {

  this->data = new Socket(dataport); // create a socket listening to that port

  // tell it the ip address to listen to.
  this->datasd = this->data->getClientSocket(this->serverIp);

  if (this->datasd == -1) {  // connection was unsuccessful
    cerr << "Error: Not connected to server." << endl;
  }

}

void Session::teardownDataSocket() {
  close(datasd);
  this->datasd = -1;

}

string Session::getDataFromServer() {
  string reply;
  // do a blocked read.
  reply = doRead(datasd);

  // poll to see if there is a server reply so we can process multiple messages.
  while (data->pollRecvFrom() > 0 && reply.compare(" ") != 0) {
    reply = doRead(datasd);
    // cout << "reply from getDataFromServer()" << endl;
  }
  return reply;
}


bool Session::getFileFromServer(string fn) {
  // need to manage the file read here
  // create a file here to read into

  string filename = fn.substr(1); // get rid of leading ' '

  // mode for get
  ofstream file;
  file.open( filename.c_str() );

  if (file.is_open()) {  // check that opening file was successful
                         // before trying to write to it.
    string reply = " ";
    // while there is data to read
    while (data->pollRecvFrom() > 0 && reply.compare("") != 0) {
      //cout << "reading file ";
      fill_n(message, BUFFERSIZE, '\0');  // clear message for this read
      read(datasd, message, BUFFERSIZE);      // Get server's reply

      // We need to find the end of the message, since it is probably shorter
      // than BUFFERSIZE.
      // A simple solution is to convert the message to a string, and then
      // get the substring that starts from the beginning of the string to the 
      // last set of terminating characters.
      string temp(message);   // convert message to string
      size_t position = temp.find_last_of("\r\n"); // find the end of the string
      if (position == -1) {
	reply = temp; // a signal to let caller know message is done.
        //cout << reply;
        file << reply;
      } else {
	reply = temp.substr(0, position); // create answer string.
        //cout << reply;
	file  << reply;  // Display reply
      }
  
    } 
    // read into the file

    file.close();  // close file
    return true;
  } else {  //opening the file was unsuccessful.
    return false;
  }
}

bool Session::sendFileToServer(string local, string remote) {
 
  // open the local file to read out of
  ifstream localfile;
  localfile.open( local.c_str(), ios::binary | ios::ate);
  int arraysize = sizeof(char) * localfile.tellg();
  localfile.close();

  localfile.open( local.c_str(), ios::binary | ios::in);


  if (localfile.is_open()) {  // check that opening file was successful
                              // before trying to read from it

    char * file; // dynamically allocated array file.
    // allocate file to be size of localfile.
   
    cout << "DEBUG: array size = " << arraysize << endl;
    file = (char *)malloc(arraysize); 
    
    if (file==NULL) {
      cout << "Null pointer exception." << endl;
    } else {

      for (int i = 0; i < arraysize; i++) {
	localfile.get(file[i]); // fill the file array.
	// cout << i << " " << file[i] << " ";
      }

      //going to need to fork the data connection for write
      //create a child to manage the data transfer.
      pid_t pid = fork();
      // let server know about the username.
      if (pid == 0) { // child process
	write(datasd, file, arraysize);
	teardownDataSocket(); // destroy data socket now we're done
	exit(0);
      } else if (pid > 0 ) { // parent
	// wait for child to finish
	int returnStatus;    
	waitpid(pid, &returnStatus, 0);
      } else { // error in fork
	cerr << "Fork error in Session!" << endl;
      }  
    }
  
    free(file);

    cout << "DEBUG: DONE!" << endl;
    // read into the file
    localfile.close();  // close file
    return true;
  } else {  //opening the file was unsuccessful.
    return false;
  }
}

char * Session::getServerIP() {
  return serverIp;
}

int Session::getDataPort() {
  return dataport;
}


string Session::doRead(int sd) {
  string reply;
  fill_n(message, BUFFERSIZE, '\0');  // clear message for this read
  read(sd, message, BUFFERSIZE);      // Get server's reply

  // We need to find the end of the message, since it is probably shorter
  // than BUFFERSIZE.
  // A simple solution is to convert the message to a string, and then
  // get the substring that starts from the beginning of the string to the 
  // last set of terminating characters.
  string temp(message);   // convert message to string
  size_t position = temp.find_last_of("\r\n"); // find the end of the string
  if (position == -1) {
    // continue
    reply = " "; // a signal to let caller know message is done.
  } else {
    reply = temp.substr(0, position); // create answer string.
    cout << reply << endl;  // Display reply
  }
  return reply;
}
