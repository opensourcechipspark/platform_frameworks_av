#include <stdio.h>
#include "datumtunel.h"

#define K 1024
#define BUFFERSIZE (63*K)

int main(int argc, char *argv[])
{
	char server_ip[]="127.0.0.1";
	char buffin[BUFFERSIZE];
	int i_read = -1;
	datumclient_start(server_ip);

	while(1){
		i_read = datumclient_read(buffin, BUFFERSIZE);
		printf("Read [%d] bytes\n", i_read);
	};

	datumclient_stop();
	return 0;
}

