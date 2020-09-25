package com.github.liutimo.capability_manage_tool.utils

import org.slf4j.LoggerFactory

class LoggerUtil {
    companion object {

        private val logger = LoggerFactory.getLogger(this.javaClass)

        fun logi(args: String) {
            logger.info(args)
        }

        fun logd(args: String) {
            logger.debug(args)
        }

        fun loge(args: String) {
            logger.error(args)
        }
    }
}