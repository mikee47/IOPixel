/*
 * IOPixel.h
 *
 *  Created on: 11 November 2018
 *      Author: mikee47
 *
 */

#pragma once

#include <IOControl.h>
#include <Libraries/Adafruit_NeoPixel/Adafruit_NeoPixel.h>

// Device configuration
DECLARE_FSTR(PIXEL_CONTROLLER_CLASSNAME)

class PixelDevice;
class PixelController;

enum PixelParameter { pixp_hue, pixp_saturation, pixp_brightness, pixp_MAX };

class PixelRequest: public IORequest
{
	friend PixelController;

public:
	PixelRequest(PixelDevice& device) :
		IORequest(reinterpret_cast<IODevice&>(device))
	{
	}

	PixelDevice& device()
	{
		return reinterpret_cast<PixelDevice&>(m_device);
	}

	ioerror_t parseJson(JsonObjectConst json) override;

	void getJson(JsonObject json) const override;

	/*
	 * We'll get called with NODES_ALL because no nodes are explictly specified.
	 */
	bool setNode(devnode_id_t nodeId) override
	{
		return (nodeId == NODES_ALL);
	}

	int operator[](PixelParameter param) const
	{
		return m_parameterValues[param];
	}

	bool contains(PixelParameter param) const
	{
		return bitRead(m_parameterMask, param);
	}

	void setParam(PixelParameter param, int value)
	{
		m_parameterValues[param] = value;
		bitSet(m_parameterMask, param);
	}

private:
	uint8_t m_parameterMask = 0; ///< Bitmask specifying which parameters are available
	int m_parameterValues[pixp_MAX] = { 0 };
};


class PixelDevice: public IODevice
{
	friend PixelController;

public:
	PixelDevice(PixelController& controller) :
		IODevice(reinterpret_cast<IOController&>(controller))
	{
	}

	~PixelDevice() override
	{
		delete m_strip;
	}

	static const device_class_info_t deviceClass();

	IORequest* createRequest() override
	{
		return new PixelRequest(*this);
	}

protected:
	ioerror_t init(JsonObjectConst config) override;
	ioerror_t execute(PixelRequest& request);
	void showTestColours();

private:
	Adafruit_NeoPixel* m_strip = nullptr;
	int m_values[pixp_MAX] = { 270, 10, 10 };
};

class PixelController: public IOController
{
public:
	PixelController(uint8_t instance) :
		IOController(instance)
	{
	}

	String classname() override
	{
		return PIXEL_CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	bool busy() const override
	{
		return m_updating;
	}

private:
	void execute(IORequest& request) override;

	bool m_updating = false; ///< Currently sending update
};
