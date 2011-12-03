#include <usb.h>
#include <stdio.h>
#include <string.h>

#define BSIZE 4000
#define CHUNK_SIZE 50

char rdata[BSIZE];

void perr (char *err, int ecode) {
	fprintf (stderr, err);
	if (ecode) exit (ecode);
}

void strange2string (char n, char *d) {
	unsigned int f = n >> 4;
	sprintf (d, "%i%i", f, n-f*16);
}

static struct usb_device *findDev(uint16_t vendor, uint16_t product) {
  struct usb_bus *bus;
  struct usb_device *dev;
  struct usb_bus *busses;

  usb_init();
  usb_find_busses();
  usb_find_devices();
  busses = usb_get_busses();

	for (bus = busses; bus; bus = bus->next)
		for (dev = bus->devices; dev; dev = dev->next)
			if ((dev->descriptor.idVendor == vendor) && (dev->descriptor.idProduct == product)) {
				return dev;
			}
	return NULL;
}

int sendData (usb_dev_handle *hdl, char *data) {
	usb_control_msg(hdl, 0xc0, 0x04, 0x500d, 0x0000, rdata, 2, 2000);
	usb_control_msg(hdl, 0x40, 0x04, 0x5001, 0x0000, data, 5, 1000);
	int r;
	int rv = 0;
	while (1) {
		r = usb_control_msg(hdl, 0xc0, 0x04, 0x5002, 0x0000, &rdata[rv], CHUNK_SIZE, 5000);
		if (r <= 0) break;
		rv += r;
	}
	return rv;
}

void readLoop (usb_dev_handle *hdl) {
	char s1[5], s2[5], s3[5];

	int rv = sendData (hdl, "\xa1\x01\x00\x03\xa5");

	int loop = 1;

	char tdata[BSIZE];

	while (1) {
		if (rv < 13) break;

		rv = sendData (hdl, "\xa1\x01\x00\x02\xa4");

		strange2string (rdata[5], s1);
		strange2string (rdata[4], s2);

		char tm[6];
		sprintf (tm, "%s:%s", s1, s2);

		int i, j;
		for (i=8; i<rv; i++) {
			tdata[i-8] = rdata[i];
		}
		j = rv-8;

		rv = sendData (hdl, "\xa1\x01\x00\x04\xa6");

		strange2string (rdata[7], s1);
		strange2string (rdata[8], s2);
		strange2string (rdata[9], s3);
		printf ("N%i training started at %s/%s/20%s %s\n", loop, s1, s2, s3, tm);

		for (i=0; i<j; i++) {
			printf ("N%i minute %i, rate %i\n", loop, i+1, (unsigned char)tdata[i]);
		}

		rv = sendData (hdl, "\xa1\x01\x00\x06\xa8");

		loop ++;
	}
}

usb_dev_handle *open_device (struct usb_device *dev) {
	usb_dev_handle *hdl;

	hdl = usb_open (dev);
	int open_status = usb_set_configuration (hdl, 1);
	open_status = usb_claim_interface (hdl, 0);
	open_status = usb_set_altinterface (hdl, 0);

	usb_control_msg(hdl, 0x40, 0x0c, 0x5003, 0x00f0, "\x10", 1, 1000); 
	usb_control_msg(hdl, 0x40, 0x0c, 0x5003, 0xffff, "\xd0", 1, 1000); 

	return hdl;
}

void close_device (usb_dev_handle *hdl) {
	if (usb_close (hdl)) {
		perr ("Cannot close USB device!\n", 1);
	}
}

void readData (usb_dev_handle *hdl) {
	sendData (hdl, "\x91\x01\x00\x01\x93");
	sendData (hdl, "\xa1\x01\x00\x01\xa3");
	readLoop (hdl);
}

void deleteData (usb_dev_handle *hdl) {
	usb_control_msg(hdl, 0xc0, 0x04, 0x500d, 0x0000, rdata, 2, 2000); 
	sendData (hdl, "\xa1\x01\x00\x00\xa2");
}

int main (int argc, char** argv) {
	struct usb_device *dev;

	if (argc < 2) {
		perr ("Should be called with one argument: 'read' or 'delete'\n", 2);
	}

	while (1) {
		if ((dev = findDev(0x0e6a, 0x0101)) == NULL) {
			perr ("Waiting for device...\n", 0);
		} else {
			break;
		}
		usleep (300000);
	}

	usb_dev_handle *hdl = open_device (dev);

	if (!strcmp(argv[1], "read")) {
		readData (hdl);
	} else if (!strcmp(argv[1], "delete")) {
		deleteData (hdl);
	} else {
		perr ("Invalid command line argument.\n", 3);
	}

	close_device (hdl);

	return 0;
}
