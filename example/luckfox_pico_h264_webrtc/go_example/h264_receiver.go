package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"
)

const (
	SocketPath = "/tmp/luckfox_h264.sock"
	HeaderSize = 32
	FrameMagic = 0x48323634 // "H264"
	FrameTypeP = 0
	FrameTypeI = 1
)

// H264FrameHeader H.264帧包头结构
type H264FrameHeader struct {
	Magic     uint32
	Timestamp uint64
	FrameType uint32
	DataLen   uint32
	Sequence  uint32
}

func main() {
	fmt.Println("========================================")
	fmt.Println("  Luckfox H.264 Receiver (Test)")
	fmt.Println("========================================")
	fmt.Printf("Connecting to: %s\n", SocketPath)

	// 等待socket文件出现
	for {
		if _, err := os.Stat(SocketPath); err == nil {
			break
		}
		fmt.Println("Waiting for C program to start...")
		time.Sleep(1 * time.Second)
	}

	// 连接到C程序
	conn, err := net.Dial("unix", SocketPath)
	if err != nil {
		log.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	fmt.Println("✓ Connected to luckfox_pico_h264_webrtc")
	fmt.Println("----------------------------------------")

	// 创建输出文件
	outFile, err := os.Create("output.h264")
	if err != nil {
		log.Fatalf("Failed to create output file: %v", err)
	}
	defer outFile.Close()

	fmt.Println("✓ Output file: output.h264")
	fmt.Println("========================================")
	fmt.Println("Receiving H.264 stream... (Press Ctrl+C to stop)")
	fmt.Println()

	// 统计信息
	var (
		frameCount  uint32
		iFrameCount uint32
		pFrameCount uint32
		totalBytes  uint64
		startTime   = time.Now()
		lastPrint   = time.Now()
	)

	// 处理退出信号
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigChan
		fmt.Println("\n\n========================================")
		fmt.Println("  Final Statistics")
		fmt.Println("========================================")
		duration := time.Since(startTime).Seconds()
		fmt.Printf("Total Frames:    %d\n", frameCount)
		fmt.Printf("I-Frames:        %d\n", iFrameCount)
		fmt.Printf("P-Frames:        %d\n", pFrameCount)
		fmt.Printf("Duration:        %.2f seconds\n", duration)
		fmt.Printf("Average FPS:     %.2f\n", float64(frameCount)/duration)
		fmt.Printf("Total Data:      %.2f MB\n", float64(totalBytes)/1024/1024)
		fmt.Printf("Average Bitrate: %.2f Mbps\n", float64(totalBytes)*8/duration/1000000)
		fmt.Println("========================================")
		fmt.Println("✓ Output saved to: output.h264")
		fmt.Println("  Play with: ffplay output.h264")
		conn.Close()
		outFile.Close()
		os.Exit(0)
	}()

	// 接收循环
	for {
		// 读取包头
		headerBuf := make([]byte, HeaderSize)
		_, err := io.ReadFull(conn, headerBuf)
		if err != nil {
			if err == io.EOF {
				fmt.Println("\n✓ Connection closed by server")
			} else {
				log.Printf("\n✗ Read header error: %v", err)
			}
			break
		}

		// 解析包头
		header := H264FrameHeader{
			Magic:     binary.LittleEndian.Uint32(headerBuf[0:4]),
			Timestamp: binary.LittleEndian.Uint64(headerBuf[4:12]),
			FrameType: binary.LittleEndian.Uint32(headerBuf[12:16]),
			DataLen:   binary.LittleEndian.Uint32(headerBuf[16:20]),
			Sequence:  binary.LittleEndian.Uint32(headerBuf[20:24]),
		}

		// 验证魔数
		if header.Magic != FrameMagic {
			log.Printf("Invalid magic: 0x%x", header.Magic)
			break
		}

		// 读取H.264数据
		data := make([]byte, header.DataLen)
		_, err = io.ReadFull(conn, data)
		if err != nil {
			log.Printf("\n✗ Read data error: %v", err)
			break
		}

		// 写入文件
		_, err = outFile.Write(data)
		if err != nil {
			log.Printf("\n✗ Write file error: %v", err)
			break
		}

		// 更新统计
		frameCount++
		totalBytes += uint64(header.DataLen)
		if header.FrameType == FrameTypeI {
			iFrameCount++
		} else {
			pFrameCount++
		}

		// 每秒打印一次统计
		if time.Since(lastPrint) >= time.Second {
			duration := time.Since(startTime).Seconds()
			fps := float64(frameCount) / duration
			bitrate := float64(totalBytes) * 8 / duration / 1000000

			frameType := "P"
			if header.FrameType == FrameTypeI {
				frameType = "I"
			}

			fmt.Printf("\r[Frame #%05d] Type: %s | Size: %6d bytes | FPS: %5.1f | Bitrate: %5.2f Mbps | I: %d P: %d",
				frameCount, frameType, header.DataLen, fps, bitrate, iFrameCount, pFrameCount)

			lastPrint = time.Now()
		}
	}

	// 最终统计
	duration := time.Since(startTime).Seconds()
	fmt.Println("\n\n========================================")
	fmt.Println("  Session Statistics")
	fmt.Println("========================================")
	fmt.Printf("Total Frames:    %d\n", frameCount)
	fmt.Printf("I-Frames:        %d\n", iFrameCount)
	fmt.Printf("P-Frames:        %d\n", pFrameCount)
	fmt.Printf("Duration:        %.2f seconds\n", duration)
	if duration > 0 {
		fmt.Printf("Average FPS:     %.2f\n", float64(frameCount)/duration)
		fmt.Printf("Average Bitrate: %.2f Mbps\n", float64(totalBytes)*8/duration/1000000)
	}
	fmt.Printf("Total Data:      %.2f MB\n", float64(totalBytes)/1024/1024)
	fmt.Println("========================================")
}
