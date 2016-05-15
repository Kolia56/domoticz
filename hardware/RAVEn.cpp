
#include "stdafx.h"
#ifndef WIN32
#include "RAVEn.h"
#include "../main/Helper.h"
#include "../main/Logger.h"
#include "hardwaretypes.h"
#include "../main/RFXtrx.h"
#include "../main/localtime_r.h"
#include "../main/mainworker.h"
#include "../tinyxpath/tinyxml.h"
#include <usb.h>
#include <stdint.h>

RAVEn::RAVEn(const int ID, const std::string& devname)
: device_(devname), wptr_(buffer_), currUsage_(0), totalUsage_(0)
{
    m_HwdID = ID;
}

RAVEn::~RAVEn(void)
{
}

bool RAVEn::StartHardware()
{
    StartHeartbeatThread();
    //Try to open the Serial Port
    try
    {
        _log.Log(LOG_STATUS, "RAVEn: Using serial port: %s", device_.c_str());
        open(device_.c_str(), 115200);
    }
    catch (boost::exception & e)
    {
        _log.Log(LOG_ERROR, "RAVEn: Error opening serial port!");
#ifdef _DEBUG
        _log.Log(LOG_ERROR, "-----------------\n%s\n-----------------", boost::diagnostic_information(e).c_str());
#else
        (void)e;
#endif
        return false;
    }
    catch (...)
    {
        _log.Log(LOG_ERROR, "RAVEn: Error opening serial port!!!");
        return false;
    }
    setReadCallback(boost::bind(&RAVEn::readCallback, this, _1, _2));
    m_bIsStarted = true;
    sOnConnected(this);

    return true;
}

bool RAVEn::StopHardware()
{
    terminate();
    StopHeartbeatThread();
    m_bIsStarted = false;
    return true;
}

void RAVEn::readCallback(const char *indata, size_t inlen)
{
    boost::lock_guard<boost::mutex> l(readQueueMutex);
    if (!m_bEnableReceive)
        return; //receiving not enabled

    const char* data;
    for(data = indata; data < indata+inlen; data++)
    {
        if(*data != '\0')
            break;
    }
    if(data == (indata+inlen))
    {
        _log.Log(LOG_ERROR, "RAVEn::readCallback only got NULLs (%d of them)", inlen);
        return;
    }
    size_t len = (indata+inlen) - data;
#ifdef _DEBUG
    _log.Log(LOG_NORM, "RAVEn::readCallback got %d, have %d.  Incoming: %s, Current: %s", len, wptr_-buffer_, data, buffer_);
#endif
    if(wptr_+len > buffer_+MAX_BUFFER_LEN)
    {
        _log.Log(LOG_ERROR, "Exceeded buffer space...resetting buffer");
        wptr_ = buffer_;
    }

    memcpy(wptr_, data, len);
    wptr_ += len;
    *wptr_ = '\0';

    TiXmlDocument doc;
    const char* endPtr = doc.Parse(buffer_);
    if (!endPtr)
    {
        _log.Log(LOG_ERROR, "RAVEn: Not enough data received (%d): %s", len, data);
        return;
    }
    else
    {
#ifdef _DEBUG
        _log.Log(LOG_NORM, "RAVEn::shifting buffer after parsing %d with %d bytes remaining: %s", endPtr - buffer_, wptr_ - endPtr, endPtr);
#endif
        memmove(buffer_, endPtr, wptr_ - endPtr);
        wptr_ = buffer_ + (wptr_ - endPtr);
    }

    TiXmlElement *pRoot;

    pRoot = doc.FirstChildElement("InstantaneousDemand");
    bool updated=false;
    if (pRoot)
    {
        currUsage_ = 1000*double(strtoul(pRoot->FirstChildElement("Demand")->GetText(), NULL, 16))/strtoul(pRoot->FirstChildElement("Divisor")->GetText(), NULL, 16);
        updated = true;
    }
    pRoot = doc.FirstChildElement("CurrentSummationDelivered");
    if(pRoot)
    {
        totalUsage_ = double(strtoul(pRoot->FirstChildElement("SummationDelivered")->GetText(), NULL, 16))/strtoul(pRoot->FirstChildElement("Divisor")->GetText(), NULL, 16);
        updated = true;
    }

    if(updated)
        SendKwhMeter(CDomoticzHardwareBase::m_HwdID, 1, 255, currUsage_, totalUsage_, "Power Meter");
    else
        _log.Log(LOG_ERROR, "RAVEn: Unknown node");
}

bool RAVEn::WriteToHardware(const char *pdata, const unsigned char length)
{
	return false;
}


#endif //WIN32
