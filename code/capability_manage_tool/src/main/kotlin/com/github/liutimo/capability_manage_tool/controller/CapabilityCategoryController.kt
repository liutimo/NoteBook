package com.github.liutimo.capability_manage_tool.controller

import com.github.liutimo.capability_manage_tool.repository.CapabilityRepository
import com.github.liutimo.capability_manage_tool.entity.CapabilityAttrCategory
import com.github.liutimo.capability_manage_tool.repository.CapabilityAttrCategoryRepository
import org.slf4j.LoggerFactory
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.data.domain.Page
import org.springframework.data.domain.PageRequest
import org.springframework.web.bind.annotation.*
import javax.sql.DataSource


@RestController
class CapabilityCategoryController {

    private val logger = LoggerFactory.getLogger(this.javaClass)


    @Autowired
    private lateinit var dataSource: DataSource

    @Autowired
    private lateinit var attributeRepository: CapabilityAttrCategoryRepository

    @PostMapping("/category/add")
    fun add(@ModelAttribute category: CapabilityAttrCategory): Boolean {

        if (category.categoryName.isEmpty) {
            return false
        }
        /**
         * 哎， 创建了一个 表专门来 保存 能力集类别
         * 此外， 还会创建一个数据表，这个表 的名字就是 类别名， 数据列就是 类别下的属性们
         */

        // [1] 保存属性
        //  判断属性是否存在
        if (attributeRepository.getCategoryByName(category.categoryName) != null) {
            //已存在
            logger.info("当前 ${category.toString()} 已存在")
            return false
        }
        attributeRepository.save(category)

        // [2] 创建一个数据表
        CapabilityRepository.getInstance(dataSource).addCategory(category.categoryName)
        return true
    }

    @GetMapping("/category/getall")
    fun getAll(): List<CapabilityAttrCategory> = attributeRepository.findAll()


    @GetMapping("/category/{page}/{size}")
    fun getPage(@PathVariable("page") page: Int,
                @PathVariable("size") size: Int): Page<CapabilityAttrCategory> {
        var mutablePage = page
        if (mutablePage >= 1) {
            mutablePage -= 1
        }

        var mutableSize = size
        if (mutableSize <= 0) {
            mutableSize = 10
        }


        val pageable = PageRequest.of(mutablePage, mutableSize)
        return attributeRepository.findAll(pageable)
    }



    @DeleteMapping("/category/{id}")
    fun deleteCategory(@PathVariable(name = "id") id: Int): Boolean {
        attributeRepository.deleteById(id)
        return true
    }


    @PostMapping("/category/update")
    fun updateCategory(@ModelAttribute category: CapabilityAttrCategory): Boolean {
        // 先判断是否存在
        val exapmle = attributeRepository.findById(category.getCategoryID())
        if (exapmle.isEmpty) {
            return false
        }

        //TODO 还要更改相应的表名称

        attributeRepository.save(category)
        return true
    }
}