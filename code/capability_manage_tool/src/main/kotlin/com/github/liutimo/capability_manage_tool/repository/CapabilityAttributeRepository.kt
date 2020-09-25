package com.github.liutimo.capability_manage_tool.repository

import com.github.liutimo.capability_manage_tool.entity.CapabilityAttribute
import org.springframework.data.jpa.repository.JpaRepository
import org.springframework.stereotype.Repository

@Repository
interface CapabilityAttributeRepository : JpaRepository<CapabilityAttribute, Int> {

}