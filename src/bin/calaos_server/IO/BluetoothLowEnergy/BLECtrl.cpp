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
#include "BLECtrl.h"
#include "Prefix.h"

BleCtrl::BleCtrl(const string &args)
{
    cDebugDom("ble") << "new BLECtrl: " << args;
    process = new ExternProcServer("ble");

    exe = Prefix::Instance().binDirectoryGet() + "/calaos_ble";

    process->messageReceived.connect(sigc::mem_fun(*this, &BleCtrl::processNewMessage));

    process->processExited.connect([=]()
    {
        //restart process when stopped
        cWarningDom("process") << "process exited, restarting...";
        process->startProcess(exe, "ble", args);
    });

    process->startProcess(exe, "ble", args);
}



BleCtrl::~BleCtrl()
{
    delete process;
}

string BleCtrl::getValue(string owid)
{
    return mapValues[owid];
}

string BleCtrl::getType(string owid)
{
    return mapTypes[owid];
}

shared_ptr<BleCtrl> BleCtrl::Instance(const string &args)
{
    static map<string, shared_ptr<BleCtrl>> mapInst;
    auto it = mapInst.find(args);
    if (it != mapInst.end())
        return it->second;

    shared_ptr<BleCtrl> inst(new BleCtrl(args));
    mapInst[args] = std::move(inst);
    return mapInst[args];
}

void BleCtrl::processNewMessage(const string &msg)
{
    Json jroot;
    try
    {
        jroot = Json::parse(msg);
        if (!jroot.is_array())
            throw (invalid_argument(string("Json is not an array: ") + msg));
    }
    catch (const std::exception &e)
    {
        cWarningDom("ble") << "Error parsing json from sub process: " << e.what();
        return;
    }

    for (auto it: jroot)
    {
        if (it.find("id") != it.end())
        {
            if (it.find("value") != it.end())
                mapValues[it["id"].get<string>()] = it["value"].get<string>();
            if (it.find("type") != it.end())
                mapTypes[it["id"].get<string>()] = it["type"].get<string>();
        }
    }

    valueChanged.emit();
}
