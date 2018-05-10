#pragma once

#include "platforms/generic.h"
#include "inputdevice.h"
#include <types/vector.h>
#include <bitset>

namespace oi {

	namespace wc {

		class Controller : public InputDevice {
			
			friend struct WindowExt;

		public:

			Controller(u32 id);
			~Controller();
			InputState getState(Binding b) const override;
			flp getAxis(Binding b) const override;

			void update(Window *w, flp dt) override;
			void vibrate(Vec2 amount, f32 time);

		protected:

			void update(Binding b, bool down) override;

			std::bitset<ControllerButton::length - 1> prev, next;
			flp axes[ControllerAxis::length - 1];
			f32 vibrationTime = 0;

		};
	}

}