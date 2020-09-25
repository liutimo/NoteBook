package com.github.liutimo.capability_manage_tool.controller

import com.alibaba.druid.spring.boot.autoconfigure.DruidDataSourceBuilder
import com.alibaba.druid.stat.DruidStatManagerFacade
import org.springframework.jdbc.core.JdbcTemplate
import org.springframework.web.bind.annotation.GetMapping
import org.springframework.web.bind.annotation.RestController
import javax.annotation.Resource
import javax.sql.DataSource

@RestController
class DruidStatController {


    @GetMapping("/druid/stat")
    fun druidStat(): Any {
        DruidDataSourceBuilder.create().build()
        return DruidStatManagerFacade.getInstance().getDataSourceStatDataList()
    }


    @Resource
    private lateinit var dataSource: DataSource

    @GetMapping("/test")
    fun test(): String {
        val jdbcTemplate = JdbcTemplate(dataSource)


        val sql = "SELECT * FROM capability_category"

        return dataSource.javaClass.toString()
    }
}