#include "writer.h"
#include <vector>
#include "animation_writer.h"

std::vector<std::pair<int, trvh>> icon_flick_queue;
std::vector<std::pair<int, trvh>> icon_state_flick_queue;

void add_flick_to_queue(trvh icon, trvh atom) {
	if (icon.type == DataType::RESOURCE) {
		icon_flick_queue.push_back(std::make_pair(icon.value, atom));
	} else if(icon.type == DataType::STRING) {
		// just in case to prevent GCing between the time of the flick and when it's written
		IncRefCount(DataType::STRING, icon.value);
		icon_state_flick_queue.push_back(std::make_pair(icon.value, atom));
	}
}

void flush_flick_queue() {
	if (icon_flick_queue.size() == 0 && icon_state_flick_queue.size() == 0) return;
	update_demo_time();
	std::vector<unsigned char> buf;
	write_vlq(buf, icon_state_flick_queue.size());
	for (int i = 0; i < icon_state_flick_queue.size(); i++) {
		std::pair<int, trvh> entry = icon_state_flick_queue[i];
		unsigned int ref = ((char)entry.second.type << 24 | entry.second.value);
		write_primitive(buf, ref);
		write_byond_string(buf, entry.first);
		DecRefCount(DataType::STRING, entry.first);
	}
	write_vlq(buf, icon_flick_queue.size());
	for (int i = 0; i < icon_flick_queue.size(); i++) {
		std::pair<int, trvh> entry = icon_flick_queue[i];
		unsigned int ref = ((char)entry.second.type << 24 | entry.second.value);
		write_primitive(buf, ref);
		write_byond_resourceid(buf, entry.first);
	}
	icon_state_flick_queue.clear();
	icon_flick_queue.clear();

	demo_file_handle.put(6); // Chunk ID
	write_vlq(buf.size());
	demo_file_handle.write((char*)&buf[0], buf.size());
}

struct AnimationFrame {
	int appearance_id = 0xFFFF;
	short flags = 0;
	unsigned char easing = 0;
	float time = 0;
};
struct Animation {
	Value atom;
	std::vector<AnimationFrame> frames;
	int loop = 1;
	int initial_appearance_id = 0xFFFF;
};
bool is_animation_continuable = false;
std::vector<Animation> animation_queue;

void populate_animate_properties(AssociativeListEntry* node, int& loop, short& easing, short& flags, float& time) {
	if (!node) return;
	if (node->left) populate_animate_properties(node->left, loop, easing, flags, time);
	if (node->right) populate_animate_properties(node->right, loop, easing, flags, time);
	if (node->key.type != DataType::STRING) return;
	if (node->value.type != DataType::NUMBER) return;
	switch (node->key.value) {
	case 244: //loop
		loop = (int)node->value.valuef;
		break;
	case 253: //easing
		easing = (unsigned char)node->value.valuef;
		break;
	case 271: //flags
		flags = (short)node->value.valuef;
		break;
	case 79: //time
		time = node->value.valuef;
		break;
	}
}

void add_animate_to_queue(trvh args, AnimatePtr oAnimate) {
	if (args.type != DataType::LIST) {
		oAnimate(args);
		return;
	}
	RawList *list = GetListPointerById(args.value);
	int loop = 1;
	short easing = 0;
	short flags = 0;
	float time = 0;
	populate_animate_properties(list->map_part, loop, easing, flags, time);
	bool is_new_animation = false;
	Value target;
	if (list->length >= 1) {
		Value item1 = list->vector_part[0];
		switch (item1.type) {
		case DataType::FILTERS:
		case DataType::AREA:
		case DataType::IMAGE:
			is_animation_continuable = false;
			oAnimate(args);
			return;
		case DataType::OBJ:
		case DataType::MOB:
		case DataType::TURF:
			target = item1;
			is_new_animation = true;
			break;
		}
		if (!target) {
			if (is_animation_continuable && !animation_queue.empty()) {
				target = animation_queue.back().atom;
			} else {
				oAnimate(args);
				return;
			}
		}
		if (flags & 4) is_new_animation = true; // ANIMATION_PARALLEL
	}
	if (is_new_animation) {
		is_animation_continuable = true;
		animation_queue.emplace_back();
	} else if (!is_animation_continuable || animation_queue.empty()) {
		oAnimate(args);
		return;
	}
	Animation &target_animation = animation_queue.back();
	if (is_new_animation) {
		target_animation.loop = loop;
		target_animation.atom = target;
		Value appearance = GetVariable(target.type, target.value, 262);
		if (appearance.type == DataType::APPEARANCE) {
			target_animation.initial_appearance_id = appearance.value;
			IncRefCount(appearance.type, appearance.value);
		}
	}
	oAnimate(args);
	Value new_appearance = GetVariable(target.type, target.value, 262);
	AnimationFrame& frame = target_animation.frames.emplace_back();
	if (new_appearance.type == DataType::APPEARANCE) {
		frame.appearance_id = new_appearance.value;
		IncRefCount(new_appearance.type, new_appearance.value);
	}
	frame.easing = easing;
	frame.flags = flags;
	frame.time = time;
}

void flush_animation_queue() {
	if (animation_queue.empty()) return;

	for (int i = 0; i < animation_queue.size(); i++) {
		Animation& animation = animation_queue[i];
		if (animation.frames.empty()) {
			// this means animation() runtimed. Since I don't think I can catch the runtime, I'm handling it here
			if (animation.initial_appearance_id != 0xFFFF) DecRefCount(DataType::APPEARANCE, animation.initial_appearance_id);
			animation_queue.erase(animation_queue.begin() + i);
			i--;
		}
	}

	if (animation_queue.empty()) return;

	update_demo_time();
	std::vector<unsigned char> buf;
	
	write_vlq(buf, animation_queue.size());
	for (int i = 0; i < animation_queue.size(); i++) {
		Animation& animation = animation_queue[i];
		int ref = ((char)animation.atom.type << 24 | animation.atom.value);
		write_primitive(buf, ref);
		write_appearance(buf, animation.initial_appearance_id);
		if (animation.initial_appearance_id != 0xFFFF) DecRefCount(DataType::APPEARANCE, animation.initial_appearance_id);
		unsigned char animation_flags = 0;
		if (!animation.frames.empty()) {
			AnimationFrame& first_frame = animation.frames[0];
			if (first_frame.flags & 1) animation_flags |= 1; // ANIMATION_END_NOW
			if (first_frame.flags & 4) animation_flags |= 2; // ANIMATION_PARALLEL
		}
		buf.push_back(animation_flags);
		unsigned short loop;
		if (animation.loop == 0) loop = 1;
		else if (animation.loop < 0) loop = 0;
		else loop = animation.loop;
		write_primitive(buf, loop);
		write_vlq(buf, animation.frames.size());
		for (int j = 0; j < animation.frames.size(); j++) {
			AnimationFrame& frame = animation.frames[j];
			write_appearance(buf, frame.appearance_id);
			if (frame.appearance_id != 0xFFFF) DecRefCount(DataType::APPEARANCE, frame.appearance_id);
			write_primitive(buf, frame.time);
			buf.push_back(frame.easing);
			unsigned char frame_flags = 0;
			if (frame.flags & 2) frame_flags |= 1; // ANIMATION_LINEAR_TRANSFORM
			buf.push_back(frame_flags);
		}
	}
	animation_queue.clear();

	demo_file_handle.put(7); // Chunk ID
	write_vlq(buf.size());
	demo_file_handle.write((char*)&buf[0], buf.size());
}
