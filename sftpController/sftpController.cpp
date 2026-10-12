#include "sftpController.hpp"


sftpController::sftpController()
{


}


sftpController::~sftpController()
{

}


bool sftpController::connect()
{

}


void sftpController::disconnect()
{

}


bool sftpController::transfer(std::string sourcePath, std::string destinationPath)
{

}


void sftpController::logging(std::string_view loggingLevel, std::string msg)
{
  std::cout << std::format("[{}][{}] {}\n", loggingLevel, logTag, msg);
}