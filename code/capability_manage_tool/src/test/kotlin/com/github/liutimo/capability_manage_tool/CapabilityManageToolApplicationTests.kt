package com.github.liutimo.capability_manage_tool

import com.github.liutimo.capability_manage_tool.repository.CapabilityRepository
import com.github.liutimo.capability_manage_tool.entity.CapabilityAttrCategory
import com.github.liutimo.capability_manage_tool.repository.CapabilityAttrCategoryRepository
import org.junit.jupiter.api.Test
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.boot.test.context.SpringBootTest
import org.springframework.jdbc.core.JdbcTemplate
import javax.annotation.Resource
import javax.sql.DataSource

@SpringBootTest
class CapabilityManageToolApplicationTests {

    @Test
    fun contextLoads() {
    }

    @Resource
    private lateinit var dataSource: DataSource

    @Test
    fun  init() {
        val jdbcTemplate = JdbcTemplate(dataSource)


        val sql = "SELECT * FROM capability_category"

        val res: MutableList<MutableMap<String, Any>> =  jdbcTemplate.queryForList(sql)

        for (item in res) {
            for((key, value) in item) {
                println("${key}:${value.toString()}")
            }
        }
    }


    @Test
    fun test_two() {
        CapabilityRepository.getInstance(dataSource).addCategory("test1")
    }

    @Test
    fun test_add_category() {
        CapabilityRepository.getInstance(dataSource).addAttribute("test1", "clo1")
    }


    @Autowired
    private lateinit var categoryRepository: CapabilityAttrCategoryRepository

    @Test
    fun test_find_category() {
        val category = CapabilityAttrCategory("123", "213")
        categoryRepository.save(category)

        val category2 = CapabilityAttrCategory("123", "")
        val res = categoryRepository.getCategoryByName("123")

        println(res.toString())
    }

    @Test
    fun test_get_attributeValueByMaterNumber() {
        val res = CapabilityRepository.getInstance(dataSource).getCategoryAttributes("123456", "dev_info")
        for ((key, value) in res) {
            println("$key + $value")
        }
    }
}
