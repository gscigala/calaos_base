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
#include "config.h"
#endif

#include "BLECtrl.h"
#include "BLETemp.h"
#include "IOFactory.h"

using namespace Calaos;

REGISTER_IO(BleTemp)

BleTemp::BleTemp(Params &p):
    InputTemp(p)
{
    // Define IO documentation for BleTemp
    ioDoc->friendlyNameSet("BleTemp");
    ioDoc->descriptionSet(_("Temperature measurement with DS18B20 Onewire Sensor"));
    ioDoc->linkAdd("Calaos Wiki", _("http://calaos.fr/wiki/OneWire"));
    ioDoc->paramAdd("ow_id", _("Unique ID of sensor on OneWire bus."), IODoc::TYPE_STRING, true);
    ioDoc->paramAdd("ow_args", _("Additional parameter used for owfs initialization."
                               "For example you can use -u to use the USB owfs drivers"),
                    IODoc::TYPE_STRING, true);

    ioDoc->paramAdd("use_w1", _("Force the use of w1 kernel driver instead of OneWire driver"), IODoc::TYPE_BOOL, false);

    ble_id = get_param("ble_id");
    ble_args = get_param("ble_args");

    // Prepend --use-w1 to blefs args if existing in conf
    string args;
    if (get_param("use_w1") == "true")
      args += "--use-w1 ";
    args += ble_args;

    ble_args = args;

    BleCtrl::Instance(ble_args);

    cDebugDom("input") << get_param("id") << ": BLE_ID : " << ble_id;
}

void BleTemp::readValue()
{
    string v = BleCtrl::Instance(ble_args)->getValue(ble_id);
    if (v.empty() || !Utils::is_of_type<double>(v))
        return;

    double val;
    Utils::from_string(v, val);
    val = Utils::roundValue(val, precision);
    if (val != value)
    {
        value = val;
        emitChange();
        cDebugDom("input") << ble_id << ": value: " << val;
    }
}
