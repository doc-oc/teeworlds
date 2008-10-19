/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <base/system.h>

#include <cstdlib>

struct PACKET
{
	PACKET *prev;
	PACKET *next;
	
	NETADDR send_to;
	int64 timestamp;
	int id;
	int data_size;
	char data[1];
};

static PACKET *first = (PACKET *)0;
static PACKET *last = (PACKET *)0;
static int current_latency = 0;
static int debug = 0;

int run(int port, NETADDR dest)
{
	NETADDR src = {NETTYPE_IPV4, {0,0,0,0},port};
	NETSOCKET socket = net_udp_create(src);
	
	char buffer[1024*2];
	int id = 0;
	
	while(1)
	{
		// handle incomming packets
		while(1)
		{
			// fetch data
			int data_trash = 0;
			NETADDR from;
			int bytes = net_udp_recv(socket, &from, buffer, 1024*2);
			if(bytes <= 0)
				break;
				
			if((rand()%2) == 0) // drop the packet
				continue;

			// create new packet				
			PACKET *p = (PACKET *)mem_alloc(sizeof(PACKET)+bytes, 1);

			if(net_addr_comp(&from, &dest) == 0)
			{
				p->send_to = src; // from the server
			}
			else
			{
				src = from; // from the client
				p->send_to = dest;
			}

			// queue packet
			p->prev = last;
			p->next = 0;
			if(last)
				last->next = p;
			else
			{
				first = p;
				last = p;
			}
			last = p;

			// set data in packet			
			p->timestamp = time_get();
			p->data_size = bytes;
			p->id = id++;
			mem_copy(p->data, buffer, bytes);
			
			if(id > 20 && bytes > 6 && data_trash)
			{
				p->data[6+(rand()%(bytes-6))] = rand()&255; // modify a byte
				if((rand()%10) == 0)
				{
					p->data_size -= rand()%32;
					if(p->data_size < 6)
						p->data_size = 6;
				}
			}

			if(debug)
				dbg_msg("crapnet", "<< %08d %d.%d.%d.%d:%5d (%d)", p->id, from.ip[0], from.ip[1], from.ip[2], from.ip[3], from.port, p->data_size);
		}
		
		//
		while(1)
		{
			//dbg_msg("crapnet", "%p", first);
			if(first && (time_get()-first->timestamp) > current_latency)
			{
				PACKET *p = first;
				first = first->next;
				if(first)
					first->prev = 0;
				else
					last = 0;
				
				if(debug)
				{
					dbg_msg("crapnet", ">> %08d %d.%d.%d.%d:%5d (%d)", p->id,
						p->send_to.ip[0], p->send_to.ip[1],
						p->send_to.ip[2], p->send_to.ip[3],
						p->send_to.port, p->data_size);
				}
				
				// send and remove packet
				//if((rand()%20) != 0) // heavy packetloss
				net_udp_send(socket, &p->send_to, p->data, p->data_size);
				
				// update lag
				double flux = rand()/(double)RAND_MAX;
				int ms_spike = 0;
				int ms_flux = 100;
				int ms_ping = 100;
				current_latency = ((time_freq()*ms_ping)/1000) + (int64)(((time_freq()*ms_flux)/1000)*flux); // 50ms
				
				if(ms_spike && (p->id%100) == 0)
					current_latency += (time_freq()*ms_spike)/1000;

				mem_free(p);
			}
			else
				break;
		}
		
		thread_sleep(1);
	}
}

int main(int argc, char **argv)
{
	NETADDR a = {NETTYPE_IPV4, {127,0,0,1},8303};
	run(8302, a);
	return 0;
}
