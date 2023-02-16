/************************************/
/* aneris project                   */
/* insert information on license    */
/* and authors here                 */
/*                                  */
/* brief description of the program */
/* as well                          */
/************************************/

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <csignal>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "gpio/GPIO.hpp"
#include "Logger/Logger.hpp"
#include "Web_Server/Web_Server.hpp"

#define WEB_SERVER_PORT 5000

#define SOCKET_PATH "/var/run/aneris.sock"
#define COMMAND_SIZE 2

#define GPIO_LIGHTS 4
#define GPIO_TEST 17
#define GPIO_HYDROPHONE 27
#define GPIO_WIPER 22

void handler(const int signum);

int main(void)
{
	/****************************/
	/* register signal handlers */
	/****************************/
	signal(SIGINT, handler);
	signal(SIGTERM, handler);
	signal(SIGABRT, handler);
	signal(SIGSEGV, handler);
	signal(SIGFPE, handler);
	signal(SIGBUS, handler);
	
	/***********************/
	/* verify user is root */
	/***********************/
	if(geteuid())
	{
		Logger::log(Logger::LOG_FATAL, "This program needs to be run as root");
		return 0;//system("reboot");
	} else Logger::log(Logger::LOG_INFO, "Verified user permission");
	
	/****************************/
	/* verify gpio is available */
	/****************************/
	try
	{
		gpio::GPIO *test_gpio = new gpio::GPIO(GPIO_TEST, gpio::GPIO_INPUT);
		if(!test_gpio) throw gpio::GPIOError("Couldn't allocate memory for 'test_gpio' variable");
		if(test_gpio->getval()) Logger::log(Logger::LOG_INFO, "Verified GPIO is available");
		else Logger::log(Logger::LOG_ERROR, "GPIO unavailable");
		delete test_gpio;
	}
	catch(gpio::GPIOError& e)
	{
		Logger::log(Logger::LOG_ERROR, e.what());
		Logger::log(Logger::LOG_ERROR, "GPIO unavailable");
	}
	
	/******************************************/
	/* test uplink to fathom tether interface */
	/*                                        */
	/* 0 on successful ping, any other        */
	/* integer otherwise                      */
	/******************************************/
	int counter = 0, tether_up = 1;
	std::string attempt = "";
	do
	{
		// this has been temporarily disabled, reenable when deploying to prod
		Logger::log(Logger::LOG_INFO, "Ping disabled");
		break;
		/*
		attempt = "Testing connection to land, attempt " + std::to_string(counter + 1);
		Logger::log(Logger::LOG_INFO, attempt);
		tether_up = system("ping -c 1 192.168.2.1");

		if(tether_up && counter < 2)
		{
			++counter;
			sleep(3);
		} else break;*/
	} while(true);
	
	if(tether_up) Logger::log(Logger::LOG_WARN, "No connection to land");
	else Logger::log(Logger::LOG_INFO, "Verified connection to land is available");
	
	/******************************************/
	/* test hydrophone is connected           */
	/*                                        */
	/* currently checks that led pin is       */
	/* enabled, but could also check that     */
	/* network card is up? TBD                */
	/******************************************/
	try
	{
		gpio::GPIO *hydrophone = new gpio::GPIO(GPIO_HYDROPHONE, gpio::GPIO_INPUT);
		if(!hydrophone) throw gpio::GPIOError("Couldn't allocate memory for 'hydrophone' variable");
		if(hydrophone->getval()) Logger::log(Logger::LOG_INFO, "Verified Hydrophone is available");
		else Logger::log(Logger::LOG_ERROR, "Hydrophone is not available");
		delete hydrophone;
	}
	catch(gpio::GPIOError& e)
	{
		Logger::log(Logger::LOG_ERROR, e.what());
		Logger::log(Logger::LOG_ERROR, "Hydrophone is not available");
	}
	
	/**************************/
	/* start command listener */
	/**************************/
	int sock, len;
	struct sockaddr_un server_sockaddr, peer_sock;
	char buf[COMMAND_SIZE];
	memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		Logger::log(Logger::LOG_FATAL, "Couldn't create socket");
		goto end;
	}

	server_sockaddr.sun_family = AF_UNIX;
	strcpy(server_sockaddr.sun_path, SOCKET_PATH); 
	len = sizeof(server_sockaddr);
	unlink(SOCKET_PATH);

	if (bind(sock, (struct sockaddr*) &server_sockaddr, len) < 0)
	{
		Logger::log(Logger::LOG_FATAL, "Couldn't bind to socket");
		goto end;
	}

	/***************************************************/
	/* start python web server (TBD, in new thread)    */
	/*                                                 */
	/* N.B. do NOT modify the 'PYTHONPATH' environment */
	/* variable, it WILL break the program             */
	/***************************************************/
	//setenv("PYTHONPATH", "./Web_Server", 1);
	//Web_Server::serve(WEB_SERVER_PORT);
	Logger::log(Logger::LOG_INFO, "Started web server");

	/*******************/
	/* begin main loop */
	/*******************/
	Logger::log(Logger::LOG_INFO, "Listening to socket");
	while (true)
	{
		memset(buf, 0, COMMAND_SIZE);
		if (recvfrom(sock, buf, COMMAND_SIZE, 0, (struct sockaddr*) &peer_sock, (socklen_t*) &len) < 0)
		{
			Logger::log(Logger::LOG_FATAL, "Couldn't receive data");
			goto end;
		}
		
		switch(atoi(buf))
		{
			case 0: // handle received commands that are not numerical and/or atoi() errors
			{
				Logger::log(Logger::LOG_ERROR, "Non numerical command received");
				break;
			}
			case 1: // shutdown
			{
				Logger::log(Logger::LOG_INFO, "Received shutdown command");

				// temporary
				goto end;

				system("poweroff");
				break;
			}
			case 2: // reboot
			{
				Logger::log(Logger::LOG_INFO, "Received reboot command");

				// temporary
				goto end;
				
				system("reboot");
				break;
			}
			case 3: // turn lights on
			{
				gpio::GPIO *lights = nullptr;
				try
				{
					lights = new gpio::GPIO(GPIO_LIGHTS, gpio::GPIO_OUTPUT);
					if(!lights) throw gpio::GPIOError("Couldn't allocate memory for 'lights' variable");
					lights->setval(gpio::GPIO_HIGH);
					Logger::log(Logger::LOG_INFO, "Enabled lights");
				}
				catch(gpio::GPIOError& e)
				{
					Logger::log(Logger::LOG_ERROR, e.what());
				}
				delete lights;
				break;
			}
			case 4: // turn lights off
			{
				gpio::GPIO *lights = nullptr;
				try
				{
					lights = new gpio::GPIO(GPIO_LIGHTS, gpio::GPIO_OUTPUT);
					if(!lights) throw gpio::GPIOError("Couldn't allocate memory for 'lights' variable");
					lights->setval(gpio::GPIO_LOW);
					Logger::log(Logger::LOG_INFO, "Disabled lights");
				}
				catch(gpio::GPIOError& e)
				{
					Logger::log(Logger::LOG_ERROR, e.what());
				}
				delete lights;
				break;
			}
			case 5: // turn wipers on/off
			{
				gpio::GPIO *wiper = nullptr;
				try
				{
					wiper = new gpio::GPIO(GPIO_WIPER, gpio::GPIO_OUTPUT);
					if(!wiper) throw gpio::GPIOError("Couldn't allocate memory for 'wiper' variable");
					if(wiper->getval())
					{
						wiper->setval(gpio::GPIO_LOW);
						Logger::log(Logger::LOG_INFO, "Disabled wiper");
					}
					else
					{
						wiper->setval(gpio::GPIO_HIGH);
						Logger::log(Logger::LOG_INFO, "Enabled wiper");
					}
				}
				catch(gpio::GPIOError& e)
				{
					Logger::log(Logger::LOG_ERROR, e.what());
				}
				delete wiper;
				break;
			}
			case 6: // clean up log file
			{
				Logger::clearLog();
				break;
			}
			case 7: {}
			case 8: {}
			default: // return that command is invalid
			{
				Logger::log(Logger::LOG_INFO, "Received an invalid command");
end: // temporary
				close(sock);
				return 0;
			}
		}
	}
}

void handler(const int signum)
{
	exit(signum);
}
