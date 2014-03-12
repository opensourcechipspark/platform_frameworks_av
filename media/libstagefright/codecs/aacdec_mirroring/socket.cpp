/****************************************************************
* 文件名称: socket.c                     
* 功    能: socket函数实现                         
* 作    者: 邱恩            2011/11/16
 ***************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h> 
#include <sys/time.h> 
#include <sys/types.h>
#include <dirent.h>  
#include <signal.h>  
#include <sys/ipc.h>
#include <sys/timeb.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>
#include <semaphore.h>
#define NONBLOCK
#include "socket.h"
using namespace android;

Socket::Socket()
{
   
}

Socket::~Socket()
{

}
int Socket::Listen(int iPort)
{
	int				iRet = 0;
	int 		    iSockFd = 0;
	int    	 		iSockFdLen = 0;

	int				iReuseAddr;

	struct sockaddr_in		szSockAddrIn;

	iSockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (iSockFd < 0) 
	{
		#ifdef DEBUG
        	printf("netLister: iSockFd [%d] error ! [%d]\iNum", iPort, iSockFd);
		#endif
        return -1;
	}

	/*设置socket可重用*/	
	iReuseAddr = SO_REUSEADDR;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_REUSEADDR, (void *)&iReuseAddr, sizeof(iReuseAddr));
	if (iRet < 0)
	{
		if (iSockFd > 0)
		{
			Close(iSockFd);
		}
  		return -2;
	}

	/*绑定端口*/
	szSockAddrIn.sin_family = AF_INET;
	szSockAddrIn.sin_addr.s_addr = htonl(INADDR_ANY);
	szSockAddrIn.sin_port = htons(iPort);

	iSockFdLen = sizeof(szSockAddrIn);
	iRet = bind(iSockFd, (struct sockaddr *)&szSockAddrIn, iSockFdLen);
	if (iRet < 0) 
	{
		#ifdef DEBUG
        	printf("netLister: bind [%d] error ! [%d]\iNum", iPort, iRet);
		#endif
		
        Close(iSockFd);

        return iRet;
	}
#ifdef NONBLOCK
	/*设置成非阻塞*/
	iRet = fcntl(iSockFd, F_SETFL, O_NONBLOCK);
	if (iRet == -1)
	{
		if (iSockFd > 0)
		{
			Close(iSockFd);
		}
		return -6;
	}
#endif
	/*监听*/
	iRet = listen(iSockFd, SOMAXCONN);
	if (iRet < 0) 
	{
		#ifdef DEBUG
        	printf("Listen: lisent [%d] error ! [%d]\iNum", iPort, iRet);
		#endif
		
        Close(iSockFd);

        return iRet;
	}

	return iSockFd;
}


int Socket::Accept(int iSrvFd)
{
	int				iRet = 0;

    int				iSockFd = 0;
    int				iSockFdLen = 0;
	struct sockaddr_in		szSockAddrIn;

	int				iReuseAddr = 0;
	int				iRecvVal = 0;
	int				iRecvLen = 0;
	int				iSendVal = 0;
	int				iSendLen = 0;

	int				iOptval = 1;

    iSockFdLen = sizeof(struct sockaddr_in);
    iSockFd = accept(iSrvFd, (struct sockaddr *)&szSockAddrIn, (socklen_t *)&iSockFdLen);
	if (iSockFd == -1)
	{
		return -1;
	}
#ifdef NONBLOCK
	//设置成非阻塞
	iRet = fcntl(iSockFd, F_SETFL, O_NONBLOCK);
	if (iRet == -1)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
		return -2;
	}
#endif
	/*设置socket可重用*/
	iReuseAddr = SO_REUSEADDR;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_REUSEADDR, (void *)&iReuseAddr, sizeof(iReuseAddr));
	if (iRet < 0)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
  		return -3;
	}

	/*设置接收缓存*/
	iRecvVal = 128 * 1024;
	iRecvLen = 4;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_RCVBUF, (void *)&iRecvVal, iRecvLen);
	if (iRet < 0)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
  		return -4;
	}

	/*设置发送缓存*/
	iSendVal = 128 * 1024;
	iSendLen = 4;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_SNDBUF, (void *)&iSendVal, iSendLen);
	if (iRet < 0)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
  		return -5;
	}

	/*设置心跳*/
	if (setsockopt(iSockFd, SOL_SOCKET, SO_KEEPALIVE, (char *)&iOptval, sizeof(iOptval)) == -1)
	{
		return -6;
	}

    return iSockFd;
}



int Socket::Close(int iSockFd)
{
	struct linger  szLig;
	szLig.l_linger = 0;
	setsockopt(iSockFd, SOL_SOCKET, SO_LINGER, (char *)&szLig, sizeof(szLig));
	close(iSockFd);
	return 1;
}

int Socket::Connect(const char *strIp, const int iPort)
{
	int					iSockFd = -1;
	int					iReuseAddr = 0;
	int					iRecvVal = 0;
	int					iRecvLen = 0;
	int					iSendVal = 0;
	int					iSendLen = 0;
	int					iFlags = 0;
	int					iRet = 0;

	int					iErr = 0;
	int					iLen = 0;

	fd_set				szFds;
	struct timeval 		    szSelectTv;
                        	
	struct sockaddr_in			szSrvAddr;

	int					iOptval = 1;
	
	iSockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (iSockFd < 0)
	{
  		return -1;
	}

	/*设置socket可重用*/
	iReuseAddr = SO_REUSEADDR;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_REUSEADDR, (void *)&iReuseAddr, sizeof(iReuseAddr));
	if (iRet < 0)
	{
		printf("setsockopt SO_REUSEADDR return error: [%s]!\n", strerror(errno));
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
  		return -2;
	}
#ifdef NONBLOCK
	/*设置成非阻塞*/
	iFlags = fcntl(iSockFd, F_GETFL, 0); 
	iRet = fcntl(iSockFd, F_SETFL, iFlags|O_NONBLOCK);
	if (iRet == -1)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
		return -3;
	}
#endif

	memset(&szSrvAddr, 0, sizeof(szSrvAddr));
	szSrvAddr.sin_family = AF_INET;
	szSrvAddr.sin_port = htons(iPort);
	szSrvAddr.sin_addr.s_addr = inet_addr((char *)strIp);

	/*连接*/
	iRet = connect(iSockFd, (struct sockaddr *)&szSrvAddr, sizeof(szSrvAddr));
	if (iRet < 0)
	{
		if (errno != EINPROGRESS)
		{
			printf("connect return fd:[%d], :[%d], EINPROGRESS: [%d], errno: [%d], error: [%s]\n", iSockFd, iRet, EINPROGRESS, errno, strerror(errno));
			if (iSockFd > 0)
			{
				close(iSockFd);
			}
			return -4;
		}
	}

	if (iRet != 0)
	{
		/*等上1秒钟看是否有连接返馈信息，即连接超时处理*/
		FD_ZERO(&szFds);
		FD_SET(iSockFd, &szFds);
		szSelectTv.tv_sec = 3;
	    szSelectTv.tv_usec = 0;
		iRet = select(iSockFd+1, NULL, &szFds, NULL, &szSelectTv);	
		if (iRet <= 0) /*5秒内没连接上端口*/
		{
			if (iSockFd > 0)
			{
				close(iSockFd);
			}
	     	return -5;
		}

		if (FD_ISSET(iSockFd, &szFds))
	    {
	        iLen = sizeof(iErr);

			iRet = getsockopt(iSockFd, SOL_SOCKET, SO_ERROR, &iErr, (socklen_t *)&iLen);
	        if (iRet < 0)
	        {
				if (iSockFd > 0)
				{
					close(iSockFd);
				}
	            return -6;
	        }
	        else
	        {
	            if (iErr != 0)
	            {
					if (iSockFd > 0)
					{
						close(iSockFd);
					}
					return -7;
	            }
	        }
	    }
	    else
	    {
			if (iSockFd > 0)
			{
				close(iSockFd);
			}
	        return -8;
	    }
	}

	/*设置接收缓存*/
	iRecvVal = 64 * 1024;
	iRecvLen = 4;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_RCVBUF, (void *)&iRecvVal, iRecvLen);
	if (iRet < 0)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
  		return -9;
	}

	/*设置发送缓存*/
	iSendVal = 64 * 1024;
	iSendLen = 4;
	iRet = setsockopt(iSockFd, SOL_SOCKET, SO_SNDBUF, (void *)&iSendVal, iSendLen);
	if (iRet < 0)
	{
		if (iSockFd > 0)
		{
			close(iSockFd);
		}
  		return -10;
	}

	/*设置心跳*/
	if (setsockopt(iSockFd, SOL_SOCKET, SO_KEEPALIVE, (char *)&iOptval, sizeof(iOptval)) == -1)
	{
		return -11;
	}

	return iSockFd;
}


int Socket::RecvN(int iSockFd, unsigned char *strBuf, int iLen, unsigned int uiTimeOut)
{
	int 		iLeft;
	int			iNum;
	unsigned char 		*p;

	//struct timeval		tvStart;
	//struct timeval		tvNow;
	/*
	if (iSockFd < 0)
	{
		return -1;
	}

	if (iLen < 1)
	{
		return 0;
	}

	if (strBuf == NULL)
	{
		return -3;
	}*/

	p = strBuf;
	iLeft = iLen;

	while (iLeft > 0)
	{
	
		/*if (uiTimeOut > 0)
		{
			gettimeofday(&tvNow, NULL);
			
			if (tvNow.tv_sec==tvStart.tv_sec && (tvNow.tv_usec/1000-tvStart.tv_usec/1000)>(int)uiTimeOut)
			{
				return -4;
			}
			
			if (tvNow.tv_sec>tvStart.tv_sec && ((tvNow.tv_sec-tvStart.tv_sec)*1000+tvNow.tv_usec/1000-tvStart.tv_usec/1000)>(int)uiTimeOut)
			{
				return -7;
			}
		}*/

		iNum = recv(iSockFd, p, iLeft, MSG_NOSIGNAL);

		if (iNum > 0)
		{
			iLeft -= iNum;
			p += iNum;
			continue;
		}

		if ((iNum==-1) && (errno==EWOULDBLOCK || errno==EINTR || errno==EAGAIN || errno==EINPROGRESS)) /*重读数据*/
		{
			usleep(5);
			continue;
		}

		if (iNum == 0)
		{
			return -5; /*connection closed*/
		}
		else
		{
			return -6;
		}
	}

	return 1;
}


int Socket::SendN(int iSockFd, unsigned char *strBuf, int iLen, unsigned int uiTimeOut)
{
	int 		iLeft;
	int			iNum;
	unsigned char 		*p;
	/*
	struct timeval		tvStart;
	struct timeval		tvNow;
	*/

	if (iSockFd < 0)
	{
		return -1;
	}

	if (iLen < 1)
	{
		return 0;
	}

	if (strBuf == NULL)
	{
		return -3;
	}

	p = strBuf;
	iLeft = iLen;
	//gettimeofday(&tvStart, NULL);

	while (iLeft > 0)
	{
		/*
		if (uiTimeOut > 0)
		{
			gettimeofday(&tvNow, NULL);

			if (tvNow.tv_sec==tvStart.tv_sec && (tvNow.tv_usec/1000-tvStart.tv_usec/1000)>(int)uiTimeOut)
			{
				return -4;
			}

			if (tvNow.tv_sec>tvStart.tv_sec && ((tvNow.tv_sec-tvStart.tv_sec)*1000+tvNow.tv_usec/1000-tvStart.tv_usec/1000)>(int)uiTimeOut)
			{
				return -7;
			}
		}
		*/

	    iNum = send(iSockFd, p, iLeft, MSG_NOSIGNAL);

	    if (iNum > 0)
	    {
			iLeft -= iNum;
			p += iNum;
			continue;
	    }

		if ((iNum==-1) && (errno==EWOULDBLOCK || errno==EINTR || errno==EAGAIN || errno==EINPROGRESS))
		{
			usleep(5);
			continue;
		}

	    if (iNum == 0)
		{
			return -5; /*connection closed*/
		}
		else
		{
			return -6;
		}
	}

	return 1;
}


int Socket::Recv(int iSockFd, unsigned char *strBuf, int iLen, unsigned int uiTimeOut)
{
	int			iRet = 0;
	struct timeval		tvSelect;
	struct timeval		*ptvSelect = NULL;
	fd_set  	fsSets;
	
	if (iSockFd < 0)
	{
		return -1;
	}
	
	if (iLen < 1)
	{
		return 0;
	}
	
	if (strBuf == NULL)
	{
		return -3;
	}

	/*
	if (uiTimeOut > 0)
	{
		tvSelect.tv_sec = uiTimeOut / 1000;
		tvSelect.tv_usec = (uiTimeOut % 1000) * 1000;
		ptvSelect = &tvSelect;
	}
	*/
	
	FD_ZERO(&fsSets);
	FD_SET(iSockFd, &fsSets);
	iRet = select(iSockFd+1, &fsSets, NULL, NULL, ptvSelect);
	switch (iRet)
	{
	case -1: /**/
		return -4;
		
		break;
	case 0:  /**/
		return 0;
		
		break;
	default : /**/
		//memset(strBuf, 0, sizeof(strBuf));
		iRet = RecvN(iSockFd, strBuf, iLen, 5);
		if (iRet < 0)
		{
			return -5*10+iRet;
		}
		
		break;
	}
	
	return 1;
}

