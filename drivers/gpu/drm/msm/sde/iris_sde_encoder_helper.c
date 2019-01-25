#if defined(CONFIG_IRIS2P_FULL_SUPPORT)
int sde_encoder_wait_for_idle(struct drm_encoder *drm_enc)
{
        struct sde_encoder_virt *sde_enc;
        struct sde_encoder_phys *phys;
        bool needs_hw_reset = false;
        uint32_t ln_cnt1, ln_cnt2;
        unsigned int i;
        int rc, ret = 0;

        if (!drm_enc || !drm_enc->dev ||
                !drm_enc->dev->dev_private) {
                SDE_ERROR("invalid args\n");
                return -EINVAL;
        }
        sde_enc = to_sde_encoder_virt(drm_enc);

        SDE_DEBUG_ENC(sde_enc, "\n");
        SDE_EVT32(DRMID(drm_enc));

        /* save this for later, in case of errors */
        if (sde_enc->cur_master && sde_enc->cur_master->ops.get_wr_line_count)
                ln_cnt1 = sde_enc->cur_master->ops.get_wr_line_count(
                                sde_enc->cur_master);
        else
                ln_cnt1 = -EINVAL;

        /* prepare for next kickoff, may include waiting on previous kickoff */
        SDE_ATRACE_BEGIN("sde_encoder_wait_for_idle");
        for (i = 0; i < sde_enc->num_phys_encs; i++) {
                phys = sde_enc->phys_encs[i];
                if (phys) {
                        if (phys->ops.prepare_for_kickoff) {
                                rc = phys->ops.prepare_for_kickoff(
                                                phys, NULL);
                                if (rc)
                                        ret = rc;
                        }
                        if (phys->enable_state == SDE_ENC_ERR_NEEDS_HW_RESET)
                                needs_hw_reset = true;
                        _sde_encoder_setup_dither(phys);
                }
        }
        SDE_ATRACE_END("sde_encoder_wait_for_idle");

        /* if any phys needs reset, reset all phys, in-order */
        if (needs_hw_reset) {
                /* query line count before cur_master is updated */
                if (sde_enc->cur_master &&
                                sde_enc->cur_master->ops.get_wr_line_count)
                        ln_cnt2 = sde_enc->cur_master->ops.get_wr_line_count(
                                        sde_enc->cur_master);
                else
                        ln_cnt2 = -EINVAL;

                SDE_EVT32(DRMID(drm_enc), ln_cnt1, ln_cnt2,
                                SDE_EVTLOG_FUNC_CASE1);
                for (i = 0; i < sde_enc->num_phys_encs; i++) {
                        phys = sde_enc->phys_encs[i];
                        if (phys && phys->ops.hw_reset)
                                phys->ops.hw_reset(phys);
                }
        }
        return ret;
}

void sde_encoder_rc_lock(struct drm_encoder *drm_enc)
{
        struct sde_encoder_virt *sde_enc;

        if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
                SDE_ERROR("invalid encoder\n");
                return;
        }
        sde_enc = to_sde_encoder_virt(drm_enc);
        mutex_lock(&sde_enc->rc_lock);
}

void sde_encoder_rc_unlock(struct drm_encoder *drm_enc)
{
        struct sde_encoder_virt *sde_enc;

        if (!drm_enc || !drm_enc->dev || !drm_enc->dev->dev_private) {
                SDE_ERROR("invalid encoder\n");
                return;
        }
        sde_enc = to_sde_encoder_virt(drm_enc);
        mutex_unlock(&sde_enc->rc_lock);
}
#endif
