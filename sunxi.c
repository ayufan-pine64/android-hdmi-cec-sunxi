#define LOG_TAG "SUNXI"

#include <hardware/hdmi_cec.h>

#include <stdlib.h>
#include <pthread.h>
#include <android/log.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ALOGE(...) do{}while(0)

#define HDMICEC_IOC_MAGIC  'H'
#define HDMICEC_IOC_SETLOGICALADDRESS _IOW(HDMICEC_IOC_MAGIC,  1, unsigned char)
#define HDMICEC_IOC_STARTDEVICE _IO(HDMICEC_IOC_MAGIC,  2)
#define HDMICEC_IOC_STOPDEVICE  _IO(HDMICEC_IOC_MAGIC,  3)
#define HDMICEC_IOC_GETPHYADDRESS _IOR(HDMICEC_IOC_MAGIC,  4, unsigned char[4])

#define CEC_SUNXI_PATH "/dev/sunxi_hdmi_cec"

typedef struct hdmi_cec_event {
  int event_type;
  int msg_len;
  unsigned char msg[17];
} hdmi_cec_event_t;

static int fd;
static int enabled = 0;
static int closed = 0;
static pthread_t process_thd = -1;
static event_callback_t callback_func;
static void* callback_arg;
static cec_logical_address_t logical_address = CEC_DEVICE_INACTIVE;

static int enable_hdmi_cec() {
	if(enabled) {
		return 0;
	}
	int ret = ioctl(fd, HDMICEC_IOC_STARTDEVICE, NULL);
	if(ret < 0) {
		ALOGE("Unable to start device: %d", ret);
	} else {
		enabled = 1;
	}
	return ret;
}

static int disable_hdmi_cec() {
	if(!enabled) {
		return 0;
	}
	int ret = ioctl(fd, HDMICEC_IOC_STOPDEVICE, NULL);
	if(ret < 0) {
		ALOGE("Unable to stop device: %d", ret);
	} else {
		enabled = 0;
	}
	return ret;
}

static int add_logical_address(const struct hdmi_cec_device* dev, cec_logical_address_t addr)
{
	if(logical_address == addr) {
		return 0;
	}
	int ret = ioctl(fd, HDMICEC_IOC_SETLOGICALADDRESS, addr);
	if(ret == 0) {
		logical_address = addr;
	}
	return -1;
}

static void clear_logical_address(const struct hdmi_cec_device* dev)
{
	add_logical_address(dev, CEC_DEVICE_INACTIVE);
}

static int get_physical_address(const struct hdmi_cec_device* dev, uint16_t* addr)
{
	return ioctl(fd, HDMICEC_IOC_GETPHYADDRESS, addr);
}

static int send_message(const struct hdmi_cec_device* dev, const cec_message_t *msg)
{
	unsigned char message[CEC_MESSAGE_BODY_MAX_LENGTH + 1];
	message[0] = (msg->initiator << 4) | (msg->destination & 0x0f);
	memcpy(message+1, msg->body, msg->length);
	return write(fd, message, msg->length + 1);
}

static void register_event_callback(const struct hdmi_cec_device* dev,
        event_callback_t callback, void* arg)
{
	callback_func = callback;
	callback_arg = arg;
}

static void get_version(const struct hdmi_cec_device* dev, int* version)
{
	*version = 0;
}

static void get_vendor_id(const struct hdmi_cec_device* dev, uint32_t* vendor_id)
{
	*vendor_id = 0;
}

static void get_port_info(const struct hdmi_cec_device* dev,
        struct hdmi_port_info* list[], int* total)
{
	static hdmi_port_info_t info = {
		.type = HDMI_OUTPUT,
		.port_id = 0,
		.cec_supported = 1,
		.arc_supported = 0,
		.physical_address = 0,
	};

	*total = 1;
	list[0] = &info;
}

static void set_option(const struct hdmi_cec_device* dev, int flag, int value)
{
	switch(flag) {
	//case HDMI_OPTION_WAKEUP:

	case HDMI_OPTION_ENABLE_CEC:
		if(value) {
			enable_hdmi_cec();
		} else {
			disable_hdmi_cec();
		}

	//case HDMI_OPTION_SYSTEM_CEC_CONTROL:

	//case HDMI_OPTION_SET_LANG:
	}
}

static void set_audio_return_channel(const struct hdmi_cec_device* dev, int port_id, int flag)
{
}

static int is_connected(const struct hdmi_cec_device* dev, int port_id)
{
	return HDMI_CONNECTED; // or HDMI_NOT_CONNECTED
}

static int close_hdmi_cec(struct hw_device_t* device)
 {
	if(fd > 0) {
		if(process_thd > 0) {
			closed = 1;
			pthread_join(process_thd, NULL);
			process_thd = 0;
		}
		disable_hdmi_cec();
		close(fd);
		fd = 0;
		return 0;
	}
	return -1;
}

static int has_data() 
{
	fd_set set;
	struct timeval timeout;
	FD_ZERO(&set);
	FD_SET(fd, &set); 

	timeout.tv_sec = 0;
  	timeout.tv_usec = 100000;

  	return select(fd + 1, &set, NULL, NULL, &timeout);
}

static void *process_thread(void *arg)
{
	hdmi_cec_event_t received_event;
	hdmi_event_t event;

	while(!closed) {
		int ret = has_data();
		if(ret == -1) {
			usleep(500*1000);
			continue;
		} else if(ret == 0) {
			continue;
		}

		ret = read(fd, &event, sizeof(hdmi_cec_event_t));
		if(ret <= 0 || received_event.msg_len < 1) {
			continue;
		}

		event.type = HDMI_EVENT_CEC_MESSAGE;
		event.cec.initiator = received_event.msg[0] >> 4;
		event.cec.destination = received_event.msg[0] & 0x0f;
		event.cec.length = received_event.msg_len - 1;
		memcpy(&event.cec.body, received_event.msg, event.cec.length);

		if(callback_func) {
			callback_func(&event, callback_arg);
		}
	}
	return NULL;
}

static int open_hdmi_cec(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
	int ret;
    hdmi_cec_device_t *dev = (hdmi_cec_device_t *) malloc(sizeof(hdmi_cec_device_t));

    if(dev == NULL)
        return -1;

	fd = open(CEC_SUNXI_PATH, O_RDWR);
	if(fd < 0) {
		ALOGE("Unable to open device: %d", errno);
		free(dev);
		return -1;
	}

    ret = pthread_create(&process_thd, NULL, process_thread, dev);
    if(ret < 0) {
		ALOGE("Unable to start thread: %d", ret);
		free(dev);
		close(fd);
		fd = 0;
		return -1;
    }

    closed = 0;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = close_hdmi_cec;
    dev->add_logical_address = add_logical_address;
    dev->clear_logical_address = clear_logical_address;
    dev->get_physical_address = get_physical_address;
    dev->send_message = send_message;
    dev->register_event_callback = register_event_callback;
    dev->get_version = get_version;
    dev->get_vendor_id = get_vendor_id;
    dev->get_port_info = get_port_info;
    dev->set_option = set_option;
    dev->set_audio_return_channel = set_audio_return_channel;
    dev->is_connected = is_connected;

    *device = (struct hw_device_t*)dev;
    return 0;
}

static struct hw_module_methods_t hdmi_cec_module_methods = {
    .open = open_hdmi_cec
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = 1,
    .hal_api_version = 0,
    .id = HDMI_CEC_HARDWARE_MODULE_ID,
    .name = "sunxi hdmi cec module",
    .author = "Kamil Trzcinski <ayufan@ayufan.eu>",
    .methods = &hdmi_cec_module_methods,
};

