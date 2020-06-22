/*
 * IOPixel.cpp
 *
 *  Created on: 11 November 2018
 *      Author: mikee47
 */

#include "IOPixel.h"
#include <ledtable.h>
#include <RGBWWLed/RGBWWLed.h>

// Device configuration
DEFINE_FSTR(PIXEL_CONTROLLER_CLASSNAME, "pixel")

// Our device name
DEFINE_FSTR_LOCAL(PIXEL_DEVICE_NAME, "pixel")
DEFINE_FSTR_LOCAL(ATTR_GPIO, "gpio")
DEFINE_FSTR_LOCAL(ATTR_HUE, "hue")
DEFINE_FSTR_LOCAL(ATTR_SAT, "saturation")
DEFINE_FSTR_LOCAL(ATTR_BRI, "brightness")

static FSTR_TABLE(attrNames) = {
	FSTR_PTR(ATTR_HUE),
	FSTR_PTR(ATTR_SAT),
	FSTR_PTR(ATTR_BRI),
};

static RGBWWColorUtils colours;

/* PixelController */

/*
 * Inherited classes call this after their own start() code.
 */
void PixelController::start()
{
	IOController::start();
}

/*
 * Inherited classes call this before their own stop() code.
 */
void PixelController::stop()
{
	IOController::stop();
}

void PixelController::execute(IORequest& request)
{
	// Apply request to owning device and pend
	auto req = reinterpret_cast<PixelRequest&>(request);
	auto err = req.device().execute(req);
	if(err < 0) {
		debug_e("Request failed, %s", ioerrorString(err).c_str());
	}
	request.complete(err);
}

/* PixelRequest */

IO::Error PixelRequest::parseJson(JsonObjectConst json)
{
	IO::Error err = IORequest::parseJson(json);
	if(err)
		return err;

	for(unsigned i = 0; i < pixp_MAX; ++i) {
		auto pp = PixelParameter(i);
		String s = *attrNames[pp];
		if(json.containsKey(s)) {
			m_parameterValues[pp] = json[s];
			bitSet(m_parameterMask, pp);
		}
	}

	return IO::Error::success;
}

void PixelRequest::getJson(JsonObject json) const
{
	IORequest::getJson(json);

	for(unsigned i = 0; i < pixp_MAX; ++i) {
		auto pp = PixelParameter(i);
		if(bitRead(m_parameterMask, pp)) {
			String s = *attrNames[pp];
			json[s] = m_parameterValues[pp];
		}
	}
}

/* PixelDevice */

static IO::Error createDevice(IOController& controller, IODevice*& device)
{
	if(!controller.verifyClass(PIXEL_CONTROLLER_CLASSNAME))
		return IO::Error::bad_controller_class;

	device = new PixelDevice(reinterpret_cast<PixelController&>(controller));
	return device ? IO::Error::success : IO::Error::nomem;
}

const device_class_info_t PixelDevice::deviceClass()
{
	return {PIXEL_DEVICE_NAME, createDevice};
}

IO::Error PixelDevice::init(JsonObjectConst config)
{
	IO::Error err = IODevice::init(config);
	if(err)
		return err;

	if(!config.containsKey(ATTR_GPIO))
		return IO::Error::bad_param;
	int gpio = config[ATTR_GPIO];

	int count = config[ATTR_COUNT];
	if(count == 0)
		return IO::Error::bad_param;

	debug_i("Pixel, GPIO = %u, count = %u", gpio, count);

	m_strip = new Adafruit_NeoPixel(count, gpio, NEO_BRG | NEO_KHZ800);
	if(m_strip == nullptr)
		return IO::Error::nomem;

	colours.setColorMode(RGB);

	m_strip->begin();
	showTestColours();

	return IO::Error::success;
}

/*
 * Distribute 24-bit colour along a string of 50 LEDs (150 in groups of 3) at the current brightness level
 *
 * We'll assume that if r + g + b = constant then perceived brightness is constant across the spectrum (not quite true but it'll do for testing).
 *
 */
void PixelDevice::showTestColours()
{
	unsigned n = m_strip->numPixels();

	/*
	int r = led(m_brightness);
	int g = 0;//led(50 * m_brightness / 256);
	int b = led(m_brightness);

	debug_i("showTestColours(%u, %u, %u), n = %u", r, g, b, n);

	for (unsigned i = 0; i < n; ++i) {
		m_strip->setPixelColor(i, r, g, b);
	}
*/

	RGBWCT rgb;
	int h = RGBWW_CALC_HUEWHEELMAX * constrain(m_values[pixp_hue], 0, 360) / 360;
	int s = RGBWW_CALC_MAXVAL * constrain(m_values[pixp_saturation], 0, 100) / 100;
	int v = RGBWW_CALC_MAXVAL * constrain(m_values[pixp_brightness], 0, 100) / 100;

	colours.HSVtoRGBrainbow(HSVCT(h, s, v), rgb);
	for(unsigned i = 0; i < n; ++i) {
		m_strip->setPixelColor(i, rgb.r, rgb.g, rgb.b);
	}

	/*
	int v = m_brightness;
	for (unsigned i = 0; i < n; ++i) {
		int h = RGBWW_CALC_HUEWHEELMAX * i / n;
		RGBWCT rgb;
		colours.HSVtoRGBrainbow(HSVCT(h, s, v), rgb);
//		rgb.r = led(rgb.r);
//		rgb.g = led(rgb.g);
//		rgb.b = led(rgb.b);
		m_strip->setPixelColor(i, rgb.r, rgb.g, rgb.b);
	}

*/

	m_strip->show();
}

IO::Error PixelDevice::execute(PixelRequest& request)
{
	switch(request.command()) {
	case ioc_query: {
		for(unsigned i = 0; i < pixp_MAX; ++i) {
			auto pp = PixelParameter(i);
			request.setParam(pp, m_values[pp]);
		}
		return IO::Error::success;
	}
	case ioc_off:
		m_values[pixp_brightness] = 0;
		break;
	case ioc_on:
		m_values[pixp_brightness] = 50;
		break;
		//	case ioc_adjust:
		//		value += request.code();
		//		break;
	case ioc_send: {
		for(unsigned i = 0; i < pixp_MAX; ++i) {
			auto pp = PixelParameter(i);
			if(request.contains(pp))
				m_values[pp] = request[pp];
		}
		break;
	}
	default:
		return IO::Error::bad_command;
	}

	showTestColours();
	return IO::Error::success;
}
