package com.github.liutimo.capability_manage_tool.controller

import com.github.liutimo.capability_manage_tool.repository.CapabilityAttrCategoryRepository
import com.github.liutimo.capability_manage_tool.repository.CapabilityAttributeRepository
import com.github.liutimo.capability_manage_tool.repository.CapabilityRepository
import com.github.liutimo.capability_manage_tool.utils.CapXMLUtil
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.web.bind.annotation.GetMapping
import org.springframework.web.bind.annotation.PathVariable
import org.springframework.web.bind.annotation.RestController
import javax.annotation.Resource
import javax.sql.DataSource

@RestController
class CapabilityFileController {

    @Resource
    private lateinit var dataSource: DataSource

    @Autowired
    private lateinit var attrCategoryRepository: CapabilityAttrCategoryRepository

    @GetMapping("/capability/get/{material_number}")
    fun getCapabilityInfo(@PathVariable(name = "material_number")materialNumber: String): String {
        //[1] 先获取所有类别
        val categories = attrCategoryRepository.findAll()

        //[2] 获取类别下的属性
        for (category in categories) {
            val attributeMap = CapabilityRepository.getInstance(dataSource).getCategoryAttributes(materialNumber, category.categoryName)

            for (attribute in category.attributes) {
                attribute.attrValue = attributeMap[attribute.attrName] as String
            }
        }

        return CapXMLUtil.convertToXML(categories)
    }
}