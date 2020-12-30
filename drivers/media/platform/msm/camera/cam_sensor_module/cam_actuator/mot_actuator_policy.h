#ifndef __MOT_ACTUATOR_POLICY_H__
#define __MOT_ACTUATOR_POLICY_H__

typedef enum {
	ACTUATOR_CLIENT_INVALID,
	ACTUATOR_CLIENT_CAMERA,
	ACTUATOR_CLIENT_VIBRATOR,
	ACTUATOR_CLIENT_MAX
} mot_actuator_client;

#define CLINET_CAMERA_MASK (0x01 << ACTUATOR_CLIENT_CAMERA)
#define CLINET_VIBRATOR_MASK (0x01 << ACTUATOR_CLIENT_VIBRATOR)

int mot_actuator_get(mot_actuator_client user);
int mot_actuator_put(mot_actuator_client user);
void mot_actuator_lock(void);
void mot_actuator_unlock(void);
ssize_t mot_actuator_dump(char *buf);
unsigned int mot_actuator_get_consumers(void);
#endif /*__MOT_ACTUATOR_POLICY_H__*/
