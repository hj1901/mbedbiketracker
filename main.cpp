#include <iostream>
#include "mbed.h"
#include "VodafoneUSBModem.h"
#include "HTTPClient.h"
#include "MBed_Adafruit_GPS.h"

DigitalOut led1(LED1), led2(LED2), led3(LED3), led4(LED4);

void post(char *url, const HTTPMap &post_data)
{
    char response_str[512];
    HTTPText response_text(response_str, 512);

    HTTPClient http;
    int error = http.post(url, post_data, &response_text);

    if (error)
    {
        cout << "Error: " << error << ", HTTP return code: "<< http.getHTTPResponseCode() << endl;
        led3 = 0;
    }
    else
    {
        cout << "Executed POST successfully - read " << strlen(response_str) << " characters" << endl;
        cout << "Response: " << response_str << endl;
        led4 = 1;
    }
}

void send_heartbeat(Adafruit_GPS *gps)
{
    // need to convert floats to strings for sending over HTTP POST
    char location_str[50], speed_str[20], angle_str[20];

    sprintf(location_str, "%5.4f%c, %5.4f%c", gps->latitude, gps->lat, gps->longitude, gps->lon);
    sprintf(speed_str, "%f", gps->speed);
    sprintf(angle_str, "%f", gps->angle);

    HTTPMap map;
    map.put("location", location_str);
    map.put("speed",    speed_str);
    map.put("angle",    angle_str);

    cout << "Sending heartbeat" << endl;
    post("http://httpbin.org/post", map);
}


// Returns true if successful
bool try_to_update_gps(Adafruit_GPS *gps)
{
    // See if a message was received
    if (!gps->newNMEAreceived())
    {
        // GPS message not received yet
        return false;
    }
    
    // If we recieved a new message from GPS, attempt to parse it
    bool parse_successful = gps->parse(gps->lastNMEA());
    if (!parse_successful)
    {
        // Bad message. We'll just wait for another one.
        return false;
    }

    if (!gps->fix)
    {
        printf(
            "GPS not yet found fix @ %d/%d/20%d %02d:%02d:%02d.\r\n",
            gps->day, gps->month, gps->year,
            gps->hour, gps->minute, gps->seconds
        );
        led2 = 0;
        return false;
    }

    printf(
        "GPS update @ %d/%d/20%d %02d:%02d:%02d. Quality: %d, Location: %5.4f%c, %5.4f%c, Speed: %5.2f knots, Angle: %5.2f, Altitude: %5.2f, Satellites: %d\r\n",
        gps->day, gps->month, gps->year,
        gps->hour, gps->minute, gps->seconds,
        (int) gps->fixquality,
        gps->latitude, gps->lat, gps->longitude, gps->lon,
        gps->speed,
        gps->angle,
        gps->altitude,
        gps->satellites
    );
    led2 = 1;

    return true;
}

void init_leds()
{
    led1 = led2 = led3 = led4 = 0;
    Thread::wait(100);
    led1 = 1;
    Thread::wait(100);
    led1 = 0, led2 = 1;
    Thread::wait(100);
    led2 = 0, led3 = 1;
    Thread::wait(100);
    led3 = 0, led4 = 1;
    Thread::wait(100);
    led4 = 0, led3 = 1;
    Thread::wait(100);
    led3 = 0, led2 = 1;
    Thread::wait(100);
    led2 = 0, led1 = 1;
    Thread::wait(100);
    led1 = 0;
}

VodafoneUSBModem *init_usb_modem()
{
    static VodafoneUSBModem usb_modem;
    int error;

    do
    {
        cout << "Connecting to mobile network..." << endl;
        error = usb_modem.connect("smart", "web", "web");    
        if (error)
        {
            cout << "Could not connect, retrying" << endl;
        }
    } while (error);

    cout << "Connected!" << endl;
    led1 = 1;
    return &usb_modem;
}

Adafruit_GPS *init_gps()
{
    static Serial gps_serial(p13, p14);
    static Adafruit_GPS gps(&gps_serial);

    // Set baud rate for GPS communication
    // This may be changed via Adafruit_GPS::sendCommand(char *)
    gps.begin(9600);  

    // A list of GPS commands is available at http://www.adafruit.com/datasheets/PMTK_A08.pdf
    // These commands are defined in MBed_Adafruit_GPS.h; a link is provided there for command creation
    gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); 
    gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    gps.sendCommand(PGCMD_ANTENNA);

    return &gps;
}

/*
 * ___MAIN___
 */
int main()
{
    cout << "== Bike Tracker ==" << endl;

    const int heartbeat_period_s = 10; // seconds    
    Timer heartbeat_timer;
    heartbeat_timer.start();
    
    init_leds();
    VodafoneUSBModem *usb_modem = init_usb_modem();
    Adafruit_GPS     *gps       = init_gps();
    
    while (1)
    {
        // Read a character from the GPS's serial stream. Each message is many
        // characters long, so we want to call this frequently.
        gps->read();

        bool gps_has_updated = try_to_update_gps(gps);

        if (gps_has_updated && heartbeat_timer.read() >= heartbeat_period_s)
        {
            led3 = 1;
            send_heartbeat(gps);
            heartbeat_timer.reset();
        }
        else if (heartbeat_timer.read() >= 1)
        {
            led3 = led4 = 0;
        }
    }
}
