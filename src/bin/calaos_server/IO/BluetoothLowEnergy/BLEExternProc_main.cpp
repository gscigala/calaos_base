/******************************************************************************
 **  Copyright (c) 2006-2017, Calaos. All Rights Reserved.
 **
 **  This file is part of Calaos.
 **
 **  Calaos is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Calaos is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ExternProc.h"

#if HAVE_LIBBLECAPI
# include <blecapi.h>
#endif

class BleProcess: public ExternProcClient
{
public:

    //needs to be reimplemented
    virtual bool setup(int &argc, char **&argv);
    virtual int procMain();

    EXTERN_PROC_CLIENT_CTOR(BleProcess)
    virtual ~BleProcess();

protected:

    //BLE specific functions
    string getValue(const string &path, const string &param);
    list<string> scanDevices();
    void sendValues();

    //needs to be reimplemented
    virtual void readTimeout();
    virtual void messageReceived(const string &msg);

private:
    bool use_w1;
};

BleProcess::~BleProcess()
{
#if HAVE_LIBBLECAPI
    Ble_finish();
#endif
}

string BleProcess::getValue(const string &path, const string &param)
{
    string value;

    if (use_w1)
    {

        if (param == "type")
            return "temperature";

        ifstream f;
        string fvalue = "/sys/bus/w1/devices/" + path + "/w1_slave";

        f.open(fvalue);
        if (!f.is_open())
            return "";

        // File read looks like that :
        // b3 01 4b 46 7f ff 0d 10 9a : crc=9a YES
        // b3 01 4b 46 7f ff 0d 10 9a t=27187

        string line;
        while(!f.eof())
        {
            getline(f, line);
            if (!line.empty())
            {
                // CRC is OK
                if (line.find("YES") != string::npos)
                {
                    // get next line and search for t=
                    size_t pos;
                    getline(f, line);
                    if (!line.empty())
                    {
                        pos = line.find("t=");
                        if (pos != string::npos)
                        {
                            pos += 2;
                            line.erase(0, pos);
                            // Divide by 1000 to get value in °C (result of read is in m°C)
                            double t;
                            from_string(line, t);
                            t /= 1000.0;
                            line = Utils::to_string(t);
                            f.close();
                            return line;

                        }
                    }
                }
            }
        }
        f.close();
        return "";
    }

#if HAVE_LIBBLECAPI

    string p = path + "/" + param;
    char *res;
    size_t len;

    if (Ble_get(p.c_str(), &res, &len) >= 0)
    {
        value = res;
        free(res);
    }
    else
        cError() << "Error reading BLE path: " << p << " " << strerror(errno);

    return value;
#else
    return "";
#endif



}

list<string> BleProcess::scanDevices()
{

    list<string> listDevices;

    if (use_w1)
    {
        ifstream f;
        string fslaves = "/sys/bus/w1/devices/w1_bus_master1/w1_master_slaves";

        f.open(fslaves);
        if (!f.is_open())
            return listDevices;
        string line;
        while(!f.eof())
        {
            getline(f, line);
            if (!line.empty())
                listDevices.push_back(line);
        }
        f.close();
        return listDevices;
    }

#if HAVE_LIBBLECAPI
    ssize_t ret;
    size_t len;
    char *dir_buffer = NULL;

    ret = Ble_get("/", &dir_buffer, &len);

    if (ret < 0)
        return listDevices;

    vector<string> tok;
    Utils::split(string(dir_buffer), tok, ",");
    free(dir_buffer);

    for (const string &s: tok)
    {
        if ((s[0] > '0' && s[0] < '9') ||
            (s[0] > 'A' && s[0] < 'F'))
        {
            if (s[s.length() - 1] == '/') //remove trailing /
                listDevices.push_back(s.substr(0, s.length() - 1));
            else
                listDevices.push_back(s);
        }
    }

    return listDevices;
#else
    return listDevices;
#endif
}

void BleProcess::readTimeout()
{
    //read all devices and send a json with all data
    list<string> l = scanDevices();

    Json jdata;
    for (const string &dev: l)
    {
        jdata.push_back({ { "id" , dev },
                          { "value", getValue(dev, "temperature") },
                          { "device_type", getValue(dev, "type") }
                        });
    }

    sendMessage(jdata.dump());
}

void BleProcess::messageReceived(const string &msg)
{
    //actually we don't need to do anything here for Ble IO.
    //calaos_server will not send us any data
    //for debug print
    cout << "Message received: " << msg << endl;
}

bool BleProcess::setup(int &argc, char **&argv)
{
    bool scan_devices = false;

    use_w1 = false;

    if (argvOptionCheck(argv, argv + argc, "--use-w1"))
    {
        cDebug() << "Use w1";
        use_w1 = true;
        argc--; argv++;
    }

    if (argvOptionCheck(argv, argv + argc, "--help") ||
        argvOptionCheck(argv, argv + argc, "-h"))
    {
        cout << "This tool is for calaos internal use only. However it may be useful " <<
                "for debugging purpose. It's designed to scan One Wire devices and print " <<
                "detected devices and their temperatures on screen" << endl << endl;
        cout << "    Usage : " << argv[0] << " [--use-w1] [--scan] <owfs_args>" << endl;
        cout << "         --use-w1    : Force the use of w1 kernel module for one wire detection." << endl;
        cout << "         --scan      : scan hardware and detect OneWire devices." << endl;
        cout << "         <owfs_args> : List of arguments used by OWFS during init (e.g. -u for usb devices)." << endl;
        return false;
    }


    if (argvOptionCheck(argv, argv + argc, "--scan"))
    {
        //this option is for direct use only and only scans all devices
        //and display a list with detected ble devices
        scan_devices = true;
        argc--; argv++;
    }
    else
    {
        if (!connectSocket())
        {
            cError() << "process cannot connect to calaos_server";
            return false;
        }
    }

    string bleargs;
    for (int i = 1;i < argc;i++)
        bleargs += string(argv[i]) + " ";

    cDebug() << "Args: " << bleargs;

#if HAVE_LIBBLECAPI
    if (!use_w1 && Ble_init(bleargs.c_str()) != 0)
    {
        cError() << "Unable to initialize Ble library : " << strerror(errno);
        return false;
    }
    if (!use_w1)
        cInfo() << "Ble Library initialization ok";
#else
    if (!use_w1)
    {
        cWarning() <<"One wire support with owfs is not build";
        return false;
    }
#endif

    if (scan_devices)
    {
        cout << "Scanning for ble devices... ";
        list<string> l = scanDevices();
        cout << "found " << l.size() << " devices." << endl;
        for (const string &dev: l)
        {
            cout << "Device: " << dev
                 << " (" << getValue(dev, "type") << ")"
                 << " --> value: " << getValue(dev, "temperature") << endl;
        }
        return false;
    }

    return true;
}

int BleProcess::procMain()
{
    //force a read+send the first time
    readTimeout();
    run(1000);

    return 0;
}

EXTERN_PROC_CLIENT_MAIN(BleProcess)
