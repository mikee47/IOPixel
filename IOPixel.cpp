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
DEFINE_FSTR(PIXEL_DEVICE_NAME, "pixel")
DEFINE_FSTR(ATTR_GPIO, "gpio")
DEFINE_FSTR(ATTR_HUE, "hue")
DEFINE_FSTR(ATTR_SAT, "saturation")
DEFINE_FSTR(ATTR_BRI, "brightness")

const FlashString* const attrNames[] PROGMEM = { FSTR_PTR(ATTR_HUE), FSTR_PTR(ATTR_SAT), FSTR_PTR(ATTR_BRI) };

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
	if (err < 0) {
		debug_e("Request failed, %s", ioerrorString(err).c_str());
		request.complete(status_error);
	}
	else {
		request.complete(status_success);
	}
}


/* PixelRequest */

ioerror_t PixelRequest::parseJson(const JsonObject& json)
{
	ioerror_t err = IORequest::parseJson(json);
	if (err)
		return err;

	for (unsigned i = 0; i < pixp_MAX; ++i) {
		auto pp = static_cast<PixelParameter>(i);
		String s = *attrNames[pp];
		if (json.containsKey(s)) {
			m_parameterValues[pp] = json[s];
			bitSet(m_parameterMask, pp);
		}
	}

	return ioe_success;
}

void PixelRequest::getJson(JsonObject& json) const
{
	IORequest::getJson(json);

	for (unsigned i = 0; i < pixp_MAX; ++i) {
		auto pp = static_cast<PixelParameter>(i);
		if (bitRead(m_parameterMask, pp)) {
			String s = *attrNames[pp];
			json[s] = m_parameterValues[pp];
		}
	}
}


/* PixelDevice */

static ioerror_t createDevice(IOController& controller, IODevice*& device)
{
	if (!controller.verifyClass(PIXEL_CONTROLLER_CLASSNAME))
		return ioe_bad_controller_class;

	device = new PixelDevice(reinterpret_cast<PixelController&>(controller));
	return device ? ioe_success : ioe_nomem;
}

const device_class_info_t PixelDevice::deviceClass()
{
	return {PIXEL_DEVICE_NAME, createDevice};
}


ioerror_t PixelDevice::init(JsonObjectConst config)
{
	ioerror_t err = IODevice::init(config);
	if (err)
		return err;

	if (!config.containsKey(ATTR_GPIO))
		return ioe_bad_param;
	int gpio = config[ATTR_GPIO];

	int count = config[ATTR_COUNT];
	if (count == 0)
		return ioe_bad_param;

	debug_i("Pixel, GPIO = %u, count = %u", gpio, count);

	m_strip = new Adafruit_NeoPixel(count, gpio, NEO_BRG | NEO_KHZ800);
	if (m_strip == nullptr)
		return ioe_nomem;

	colours.setColorMode(RGB);

	m_strip->begin();
	showTestColours();

	return ioe_success;
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
	for (unsigned i = 0; i < n; ++i) {
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

ioerror_t PixelDevice::execute(PixelRequest& request)
{
	switch(request.command()) {
	case ioc_query: {
		for (unsigned i = 0; i < pixp_MAX; ++i) {
			auto pp = static_cast<PixelParameter>(i);
			request.setParam(pp, m_values[pp]);
		}
		return ioe_success;
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
		for (unsigned i = 0; i < pixp_MAX; ++i) {
			auto pp = static_cast<PixelParameter>(i);
			if (request.contains(pp))
				m_values[pp] = request[pp];
		}
		break;
	}
	default:
		return ioe_bad_command;
	}

	showTestColours();
	return ioe_success;
}
