package com.github.liutimo.capability_manage_tool.repository

import com.github.liutimo.capability_manage_tool.utils.LoggerUtil
import org.springframework.jdbc.core.JdbcTemplate
import javax.sql.DataSource
import kotlin.reflect.jvm.internal.impl.load.java.descriptors.AnnotationDefaultValue

class CapabilityRepository private constructor(private val jdbcTemplate: JdbcTemplate){

    companion object {

        private var repository: CapabilityRepository? = null

        fun getInstance(dataSource: DataSource) : CapabilityRepository {
            if (repository == null) {
                repository = CapabilityRepository(JdbcTemplate(dataSource))
            }

            return repository!!
        }
    }


    /**
     * 我们的能力集目前有 5大类：
     * 1. 能力集文件信息
     * 2. 设备基本信息
     * 3. 设备默认参数
     * 4. 能力与组件支持
     * 5. 视频播放能力集
     * 每一个类别对应一个数据表。
     * 每一个类别下的 属性 对应这个数据表中的列
     */
    fun addCategory(categoryName: String) {
        val sql =   "create TABLE $categoryName (\n" +
                    " id int auto_increment,\n" +
                    " material_number varchar(16) not null unique,\n" +
                    " primary key (id)\n" +
                    ");"

        try {
            jdbcTemplate.execute(sql)
        } catch (e: Exception) {
            e.printStackTrace()
            LoggerUtil.logi("表($categoryName)已经存在!")
        }
    }


    /**
     * 添加一个属性
     * 每一个属性需要只在其对应的 类别表中增加一个字符
     */
    fun addAttribute(categoryName: String, attributeName: String, defaultValue: String = "0") {
        val sql =   "ALTER TABLE $categoryName ADD $attributeName VARCHAR(120) DEFAULT $defaultValue"
        jdbcTemplate.execute(sql)
    }


    /**
     * 删除一个属性
     * 对应的，我们也要在 其对应的表中 删除关联 字段
     */
    fun delAttribute(categoryName: String, attributeName: String) {
        val sql = "ALTER TABLE $categoryName DROP COLUMN $attributeName"
        jdbcTemplate.execute(sql)
    }


    /***********************************
     * 能力集文件 有多个类别 ， 每个类别对应多个属性
     *
     * 从 CapabilityAttrCateory中获取所有类别
     * 从 CapabilityAttribute中获取与类别关联的属性。
     *
     * 最后， 获取每个能力集文件 关联的 属性值 并生成 xml文件
     *
     * 根据 类别名查询到 关联 数据表中 与能力集文件关联的所有字段及值 Map<String, Any>
     *     然后根据属性值到map里面去取值。。。
     */

    /**
     * 查询能力集文件的某个类别下的所有属性值
     * @param materialNumber 物料号
     * @param categoryName 类别名称
     */
    fun getCategoryAttributes(materialNumber: String, categoryName: String): Map<String, Any> {
        val sql = "SELECT * FROM $categoryName WHERE material_number=$materialNumber "
        return jdbcTemplate.queryForMap(sql)
    }



}