#ifndef __IRIS_SDE_ENCODER_HELPER_H__
#define __IRIS_SDE_ENCODER_HELPER_H__

#if defined(CONFIG_IRIS2P_FULL_SUPPORT)
/**
 * sde_encoder_wait_for_idle - wait for dhpy to be idle
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if successful in waiting for dhpy in idle.
 */
int sde_encoder_wait_for_idle(struct drm_encoder *drm_enc);

/**
 * sde_encoder_rc_lock - lock the sde encoder resource control.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     void.
 */
void sde_encoder_rc_lock(struct drm_encoder *drm_enc);

/**
 * sde_encoder_rc_unlock - unlock the sde encoder resource control.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     void.
 */
void sde_encoder_rc_unlock(struct drm_encoder *drm_enc);
#endif

#endif /* __IRIS_SDE_ENCODER_HELPER_H__ */
