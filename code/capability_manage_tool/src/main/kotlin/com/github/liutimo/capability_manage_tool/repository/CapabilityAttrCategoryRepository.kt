package com.github.liutimo.capability_manage_tool.repository

import com.github.liutimo.capability_manage_tool.entity.CapabilityAttrCategory
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.data.jpa.repository.Query
import org.springframework.stereotype.Repository

@Repository
interface CapabilityAttrCategoryRepository : JpaRepository<CapabilityAttrCategory, Int> {

    //根据categoryName 查询

    @Query(value = "select category from CapabilityAttrCategory category where category.categoryName=?1")
    fun getCategoryByName(categoryName: String): CapabilityAttrCategory?

}