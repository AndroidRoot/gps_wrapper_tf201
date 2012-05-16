/*
 * Copyright 2012 AndroidRoot.mobi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
* limitations under the License.
*/

#define LOG_TAG  "gps_tf201"
#define LOG_NDEBUG 0

#include <hardware/gps.h>
#include <hardware/hardware.h>
#include <utils/Log.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <cutils/properties.h>

static GpsInterface interface_wrapper;
static GpsInterface* interface_internal = NULL;
static GpsInterface* interface_external = NULL;
static GpsCallbacks* device_callbacks = NULL;

//Use 0 for internal and 1 for external
static int current_type = 0;

static void wrapper_current_type() {

    char value[128];
    property_get("persist.sys.asus.gps",value,"0");
    if (!strcmp("external", value)) {
        current_type = 1;
	return;
    } else if (!strcmp("internal", value)) {
        current_type = 0;
	return;
    }

    struct stat st;
    if(stat("/dev/ttyACM0",&st) == 0){
        LOGI("External GPS Present (/dev/ttyACM0)");
        current_type = 1;
        return;
    } else {
        LOGI("No External GPS Found");
        current_type = 0;
        return;
    }

}

static void current_device_check(){
    int old_type = current_type;
    wrapper_current_type();

    LOGI("Check Current Device");

    if (old_type == current_type){
        LOGV("No Device Switch Needed");
        return;
    }

    if (current_type == 0){
        LOGV("Switch External -> Internal");
        interface_external->cleanup();
        interface_internal->init(device_callbacks);
                
    } else if (current_type == 1) {
        LOGV("Switch Internal -> External");
        interface_internal->cleanup();
        interface_external->init(device_callbacks);
    }

    return;
}

static int wrapper_init(GpsCallbacks* callbacks) {

    wrapper_current_type();
    device_callbacks = callbacks;

    if (current_type ==0) {
        LOGI("Wrapper init (internalGPS)");
        interface_internal->init(callbacks);
    } else {
        LOGI("Wrapper init (externalGPS)");
        interface_external->init(callbacks);
    }

    return 0;
}

static int wrapper_start() {
    LOGV("Wrapper start");

    current_device_check();

    if (current_type ==0)
       interface_internal->start();
    else
       interface_external->start();

    return 0;
}

static int wrapper_stop() {
    LOGV("Wrapper stop");

    if (current_type ==0)
        interface_internal->stop();
    else
        interface_external->stop();

    current_device_check();

    return 0;
}

static void wrapper_cleanup() {
    LOGV("Wrapper cleanup");

    if (current_type ==0)
        interface_internal->cleanup(); 
    else
        interface_external->cleanup();
}

static int wrapper_inject_time(GpsUtcTime time, int64_t timeReference,
                         int uncertainty) {
    LOGV("Wrapper inject time");

    if (current_type ==0)
        interface_internal->inject_time(time, timeReference, uncertainty);
    else
        interface_external->inject_time(time, timeReference, uncertainty);

    return 0;
}

static int wrapper_inject_location(double latitude, double longitude, float accuracy) {
    LOGV("Wrapper inject location");

    if (current_type ==0)
        interface_internal->inject_location(latitude, longitude, accuracy);
    else
        interface_external->inject_location(latitude, longitude, accuracy);

    return 0;
}

static void wrapper_delete_aiding_data(GpsAidingData flags) {
    LOGV("Wrapper delete aiding data");

    if (current_type ==0)
        interface_internal->delete_aiding_data(flags);
    else
        interface_external->delete_aiding_data(flags);
}

static int wrapper_set_position_mode(GpsPositionMode mode, GpsPositionRecurrence recurrence,
uint32_t min_interval, uint32_t preferred_accuracy, uint32_t preferred_time) {
    LOGV("Wrapper set position mode");

    if (current_type ==0)
        interface_internal->set_position_mode(mode, recurrence, min_interval, preferred_accuracy,
                preferred_time);
    else
        interface_external->set_position_mode(mode, recurrence, min_interval, preferred_accuracy,
                preferred_time);
return 0;
}

static const void* wrapper_get_extension(const char* name) {
    LOGV("Wrapper get extension");

    if (current_type ==0)
        interface_internal->get_extension(name);
    else
        interface_external->get_extension(name);

return NULL;
}

const GpsInterface* get_wrapper_interface(struct gps_device_t* dev)
{
    interface_wrapper.size = sizeof(GpsInterface);
    interface_wrapper.init = wrapper_init;
    interface_wrapper.start = wrapper_start;
    interface_wrapper.stop = wrapper_stop;
    interface_wrapper.cleanup = wrapper_cleanup;
    interface_wrapper.inject_time = wrapper_inject_time;
    interface_wrapper.inject_location = wrapper_inject_location;
    interface_wrapper.delete_aiding_data = wrapper_delete_aiding_data;
    interface_wrapper.set_position_mode = wrapper_set_position_mode;
    interface_wrapper.get_extension = wrapper_get_extension;

    return &interface_wrapper;
}

static int open_wrapper(const struct hw_module_t* module, char const* name,
    struct hw_device_t** device) 
{
    hw_device_t *dev_internal;
    hw_device_t *dev_external;
    int status = 0;
    void *module_internal;
    void *module_external;
    hw_module_t* hw_module_internal;
    hw_module_t* hw_module_external;
    struct gps_device_t* gps_device_internal;
    struct gps_device_t* gps_device_external;
    struct gps_device_t* gps_device_wrapper;

    LOGI("TF201 GPS Wrapper Open");

    wrapper_current_type();

    module_internal = dlopen("/system/lib/hw/gpsinternal.tegra.so", RTLD_NOW);
    if (module_internal == NULL) {
        LOGE("error loading module");
        status -1;
    }

    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    hw_module_internal = (struct hw_module_t *)dlsym(module_internal, sym);
    if (hw_module_internal == NULL) {
        LOGE("couldn't find symbol %s", sym);
        status = -1;
    }

    if (status == 0 ){
        LOGI("loading internal GPS HAL name=%s id=%s", hw_module_internal->name, hw_module_internal->id);
        hw_module_internal->methods->open(hw_module_internal, GPS_HARDWARE_MODULE_ID, &dev_internal);
    } else {
        LOGE("Error loading internal GPS HAL");
        module_internal = NULL;
    }

    if (status == 0){
        gps_device_internal = (struct gps_device_t *)dev_internal;
        interface_internal = (GpsInterface *)gps_device_internal->get_gps_interface(gps_device_internal);
    }

    module_external = dlopen("/system/lib/hw/gpsdongle.tegra.so", RTLD_NOW);
    if (module_external == NULL) {
        char const *err_str = dlerror();
        LOGE("error loading module");
        status -1;
    }

    hw_module_external = (struct hw_module_t *)dlsym(module_external, sym);
    if (hw_module_external == NULL) {
        LOGE("couldn't find symbol %s", sym);
        status = -1;
    }

    if (status == 0 ){
        LOGI("loading external GPS HAL name=%s id=%s", hw_module_external->name, hw_module_external->id);
        hw_module_external->methods->open(hw_module_external, GPS_HARDWARE_MODULE_ID, &dev_external);
    } else {
        LOGE("Error loading external GPS HAL");
        module_external = NULL;
    }

    if (status == 0){

        gps_device_external = (struct gps_device_t *)dev_external;
        interface_external = (GpsInterface *)gps_device_external->get_gps_interface(gps_device_external);

        gps_device_wrapper = malloc (sizeof(struct gps_device_t));
        memset(gps_device_wrapper, 0 , sizeof(*gps_device_wrapper));

        gps_device_wrapper->common.tag = HARDWARE_DEVICE_TAG;
        gps_device_wrapper->common.version = 1;
        gps_device_wrapper->common.module = (struct hw_module_t*)module;
        gps_device_wrapper->get_gps_interface = get_wrapper_interface;

        *device = (struct hw_device_t*)gps_device_wrapper;

    } else {
        LOGE("Error loading tf201 GPS wrapper");
    }

    return status;
}

static struct hw_module_methods_t gps_module_methods = {
    .open = open_wrapper
};

const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 0,
    .version_minor = 2,
    .id = GPS_HARDWARE_MODULE_ID,
    .name = "TF201 GPS Wrapper",
    .author = "AndroidRoot.mobi",
    .methods = &gps_module_methods,
};

